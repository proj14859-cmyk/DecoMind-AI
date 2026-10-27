"""Proveedor Groq (gratis, API compatible con OpenAI). Solo biblioteca estandar.

Variables de entorno (o archivo Server/.env):
    GROQ_API_KEY   clave de Groq (obligatoria)
    GROQ_MODEL     por defecto llama-3.3-70b-versatile (si no existe, se elige otro)
    GROQ_MAX_OBJS  tope de objetos por peticion (por defecto 300)
"""
import json
import os
import re
import urllib.error
import urllib.request

from .catalog import Catalog
from .prompts import build_system
from .styles import style_block
from .protocol import ApiReply, ApiRequest
from .providers import Provider, register_provider

URL = "https://api.groq.com/openai/v1/chat/completions"
MODELS_URL = "https://api.groq.com/openai/v1/models"
# Orden de preferencia si el modelo configurado no existe en tu cuenta.
PREFERRED = ["openai/gpt-oss-120b", "llama-3.3-70b-versatile", "openai/gpt-oss-20b", "llama-3.1-8b-instant"]

# Rangos que tambien valida el mod (LevelValidator).
MAX_X, MAX_Y = 200000, 4000
NO_OBJECT_MODES = {"ideas"}
# Claves de objeto GD permitidas en "props" y su rango (min, max).
PROP_RANGES = {
    "7": (0, 255), "8": (0, 255), "9": (0, 255),      # R, G, B
    "10": (0, 60), "45": (0, 60), "46": (0, 60), "47": (0, 60),  # duracion / fade in-hold-out
    "21": (0, 1105), "22": (0, 1105), "23": (0, 1105),  # canales de color
    "24": (-10, 15), "25": (-500, 500),                # z-layer, z-order
    "28": (-3000, 3000), "29": (-3000, 3000),          # mover X / Y
    "30": (0, 18), "85": (0.1, 10),                    # easing / rate
    "35": (0, 1),                                      # opacidad
    "51": (0, 9999), "71": (0, 9999),                  # grupo objetivo / centro
    "52": (0, 5), "20": (0, 9999), "61": (0, 9999),    # tipo objetivo, editor layers
    "68": (-36000, 36000), "69": (-36000, 36000),      # rotar grados / vueltas
    "150": (0.05, 20), "151": (0.05, 20),              # escala X / Y
    "4": (0, 1), "5": (0, 1), "11": (0, 1), "17": (0, 1), "56": (0, 1),
    "62": (0, 1), "64": (0, 1), "67": (0, 1), "87": (0, 1),  # flags
}
INT_PROPS = {"7", "8", "9", "21", "22", "23", "24", "25", "51", "71", "30", "52", "20", "61",
             "4", "5", "11", "17", "56", "62", "64", "67", "87"}
_GROUPS_RE = re.compile(r"^\d{1,4}(\.\d{1,4}){0,9}$")  # "57": "20.21.22"


def _load_env_file():
    path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), ".env")
    if not os.path.isfile(path):
        return
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            k, _, v = line.partition("=")
            os.environ.setdefault(k.strip(), v.strip().strip('"').strip("'"))


def extract_json(text: str):
    """Obtiene el primer objeto JSON del texto (tolera ```json ... ``` y texto extra)."""
    text = text.strip()
    fence = re.search(r"```(?:json)?\s*(.*?)```", text, re.S)
    if fence:
        text = fence.group(1).strip()
    start, end = text.find("{"), text.rfind("}")
    if start == -1 or end <= start:
        raise ValueError("la IA no devolvio JSON")
    return json.loads(text[start:end + 1])


def build_objects(raw, mode: str, max_objs: int, catalog: Catalog):
    """Valida los objetos de la IA contra el catalogo y los convierte al formato GD.
    Devuelve (cadena, descartados)."""
    out, dropped = [], 0
    if not isinstance(raw, list):
        return "", 0
    for o in raw:
        if not isinstance(o, dict) or len(out) >= max_objs:
            dropped += 1
            continue
        try:
            oid, x, y = int(o["id"]), float(o["x"]), float(o["y"])
            rot = float(o.get("rot", 0) or 0)
            scale = float(o.get("scale", 1) or 1)
        except (KeyError, TypeError, ValueError):
            dropped += 1
            continue
        if not catalog.is_allowed(mode, oid) or not (0 <= x <= MAX_X) or not (-300 <= y <= MAX_Y):
            dropped += 1
            continue
        s = f"1,{oid},2,{x:g},3,{y:g}"
        if rot:
            s += f",6,{rot:g}"
        if scale != 1:
            s += f",32,{min(10.0, max(0.1, scale)):g}"
        props = o.get("props")
        if isinstance(props, dict):
            for k, v in props.items():
                k = str(k)
                if k == "57":  # grupos: entero o "20.21.22" (1..9999, maximo 10)
                    g = str(v).strip()
                    if _GROUPS_RE.match(g) and all(1 <= int(n) <= 9999 for n in g.split(".")):
                        s += f",57,{g}"
                    continue
                if k not in PROP_RANGES:
                    continue
                try:
                    v = float(v)
                except (TypeError, ValueError):
                    continue
                lo, hi = PROP_RANGES[k]
                v = min(hi, max(lo, v))
                s += f",{k},{int(v) if k in INT_PROPS else format(v, 'g')}"
        out.append(s + ";")
    return "".join(out), dropped


class GroqProvider(Provider):
    name = "groq"

    def __init__(self):
        _load_env_file()
        self.key = os.environ.get("GROQ_API_KEY", "").strip()
        self.model = os.environ.get("GROQ_MODEL", "llama-3.3-70b-versatile").strip()
        self.max_objs = int(os.environ.get("GROQ_MAX_OBJS", "300"))
        self.catalog = Catalog()
        if not self.key:
            print("[aviso] Falta GROQ_API_KEY: el proveedor groq respondera con un error claro.")

    def _chat(self, messages) -> str:
        body = json.dumps({
            "model": self.model, "messages": messages,
            "temperature": 0.4, "max_tokens": 6000,
        }).encode("utf-8")
        req = urllib.request.Request(URL, data=body, headers={
            "Authorization": f"Bearer {self.key}",
            "Content-Type": "application/json",
            "User-Agent": "DecoMind-AI/0.3",
        })
        with urllib.request.urlopen(req, timeout=40) as resp:
            data = json.loads(resp.read().decode("utf-8"))
        choice = data["choices"][0]
        text = choice["message"].get("content") or ""
        print(f"[groq] modelo={self.model} finish={choice.get('finish_reason')} chars={len(text)}")
        return text

    def _pick_available_model(self) -> bool:
        """Consulta /models y elige el primer modelo preferido disponible. True si cambio el modelo."""
        r = urllib.request.Request(MODELS_URL, headers={
            "Authorization": f"Bearer {self.key}", "User-Agent": "DecoMind-AI/0.3"})
        with urllib.request.urlopen(r, timeout=20) as resp:
            ids = {m.get("id") for m in json.loads(resp.read().decode("utf-8")).get("data", [])}
        for cand in PREFERRED:
            if cand in ids and cand != self.model:
                print(f"[groq] modelo {self.model!r} no disponible; uso {cand!r}")
                self.model = cand
                return True
        return False

    def generate(self, req: ApiRequest) -> ApiReply:
        if not self.key:
            return ApiReply("Falta la clave de Groq. Define GROQ_API_KEY en el servidor (ver README).")
        user = (f"Modo: {req.mode}\nNivel: {req.level or '(sin nombre)'}\n"
                f"Instrucciones del modo: {req.context}\n"
                f"Contexto del nivel:\n{req.format_editor_perception()}\n"
                f"Peticion del usuario: {req.prompt}")
        self.catalog.reload()  # recoge un catalogo nuevo subido por el mod sin reiniciar
        cats = self.catalog.allowed_cats(req.mode)
        note = ""
        if "deco" in cats and not self.catalog.game_loaded:
            note = ("AVISO: no hay catalogo del juego cargado, asi que no tienes IDs [deco]. "
                    "Dile al usuario que escriba /catalogo en el chat del mod.")
        style = style_block(req.prompt, req.mode, self.catalog)
        if style:
            print("[groq] estilo detectado en el pedido")
        system = build_system(req.mode, self.max_objs, self.catalog.describe(req.mode), note, style)
        messages = [
            {"role": "system", "content": system},
            {"role": "user", "content": user},
        ]
        try:
            try:
                text = self._chat(messages)
            except urllib.error.HTTPError as e0:
                if e0.code in (400, 404) and self._pick_available_model():
                    text = self._chat(messages)
                else:
                    raise
        except urllib.error.HTTPError as e:
            if e.code == 429:
                return ApiReply("Groq esta limitando las peticiones (429). Espera un minuto e intenta de nuevo.")
            if e.code in (401, 403):
                return ApiReply("Groq rechazo la clave (401/403). Revisa GROQ_API_KEY.")
            detail = ""
            try:
                err = json.loads(e.read().decode("utf-8", errors="replace"))
                detail = str(err.get("error", {}).get("message", ""))[:240]
            except Exception:
                pass
            print(f"[groq] HTTP {e.code} (modelo {self.model}): {detail}")
            return ApiReply(f"Groq devolvio un error HTTP {e.code}: {detail}")
        except (urllib.error.URLError, TimeoutError):
            return ApiReply("No se pudo contactar con Groq. Revisa la conexion a internet del servidor.")

        try:
            data = extract_json(text)
        except ValueError:
            print(f"[groq] respuesta sin JSON valido: {text[:300]!r}")
            return ApiReply("La IA no devolvio un formato valido (o se quedo sin espacio). Intenta de nuevo con una peticion mas corta.")

        msg = str(data.get("message", "")).strip() or "Listo."
        print(f"[groq] mensaje de la IA: {msg[:200]!r}")
        if req.mode in NO_OBJECT_MODES:
            return ApiReply(msg)
        raw_objs = data.get("objects")
        objs, dropped = build_objects(raw_objs, req.mode, self.max_objs, self.catalog)
        n_in = len(raw_objs) if isinstance(raw_objs, list) else 0
        print(f"[groq] objetos: {n_in} recibidos, {objs.count(';')} validos, {dropped} descartados")
        if dropped and n_in:
            sample = [o.get("id") for o in raw_objs if isinstance(o, dict)][:8]
            print(f"[groq] ids pedidos por la IA (primeros): {sample}")
        if dropped:
            msg += f" (Se descartaron {dropped} objetos no validos.)"
        if not objs:
            msg += " No se coloco ningun objeto."
        return ApiReply(msg, objs)


register_provider("groq", GroqProvider)
