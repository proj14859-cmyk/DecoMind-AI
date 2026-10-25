"""Proveedor Groq (gratis, API compatible con OpenAI). Solo biblioteca estandar.

Variables de entorno (o archivo Server/.env):
    GROQ_API_KEY   clave de Groq (obligatoria)
    GROQ_MODEL     por defecto llama-3.3-70b-versatile
    GROQ_MAX_OBJS  tope de objetos por peticion (por defecto 300)
"""
import json
import os
import re
import urllib.error
import urllib.request

from .protocol import ApiReply, ApiRequest
from .providers import Provider, register_provider

URL = "https://api.groq.com/openai/v1/chat/completions"

# Rangos que tambien valida el mod (LevelValidator).
MAX_ID = 4600
MAX_X, MAX_Y = 200000, 4000
# Modos que SOLO pueden decorar: no se permiten picos ni portales (protege el gameplay).
LAYOUT_MODES = {"layout", "layout_deco", "estructurar", "block_design"}
NO_OBJECT_MODES = {"ideas"}
GAMEPLAY_IDS = {8, 9, 39, 103, 392,                       # picos
                10, 11, 12, 13, 45, 46, 47, 99, 101, 111,  # portales (gravedad/forma/espejo/tamaño)
                286, 287, 660, 745}                        # dual / wave / robot

SYSTEM = """Eres DecoMind AI, asistente del editor de Geometry Dash (2.2).
Responde SOLO con un objeto JSON valido, sin texto fuera del JSON, con esta forma:
{"message": "explicacion breve en espanol de lo que hiciste",
 "objects": [{"id": 1, "x": 300, "y": 15, "rot": 0}]}
Reglas:
- Coordenadas en unidades del editor: 1 bloque = 30. El suelo es y=15 (primera fila), luego 45, 75, 105...
- x empieza en 300 y avanza hacia la derecha. "rot" es opcional (grados).
- id 1 = bloque solido basico, id 8 = pico. Usa solo IDs de objeto que existan en GD.
- Maximo {max_objs} objetos. Si no hay nada que colocar, devuelve "objects": [].
- Nunca dejes un tramo imposible de pasar en el modo layout.
"""


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


def build_objects(raw, mode: str, max_objs: int):
    """Valida los objetos de la IA y los convierte a '1,<id>,2,<x>,3,<y>[,6,<rot>];'. Devuelve (cadena, descartados)."""
    out, dropped = [], 0
    if not isinstance(raw, list):
        return "", 0
    for o in raw:
        if len(out) >= max_objs:
            dropped += 1
            continue
        try:
            oid, x, y = int(o["id"]), float(o["x"]), float(o["y"])
            rot = float(o.get("rot", 0) or 0)
        except (KeyError, TypeError, ValueError):
            dropped += 1
            continue
        if not (1 <= oid <= MAX_ID) or not (0 <= x <= MAX_X) or not (-300 <= y <= MAX_Y):
            dropped += 1
            continue
        if mode not in LAYOUT_MODES and oid in GAMEPLAY_IDS:
            dropped += 1
            continue
        s = f"1,{oid},2,{x:g},3,{y:g}"
        if rot:
            s += f",6,{rot:g}"
        out.append(s + ";")
    return "".join(out), dropped


class GroqProvider(Provider):
    name = "groq"

    def __init__(self):
        _load_env_file()
        self.key = os.environ.get("GROQ_API_KEY", "").strip()
        self.model = os.environ.get("GROQ_MODEL", "llama-3.3-70b-versatile").strip()
        self.max_objs = int(os.environ.get("GROQ_MAX_OBJS", "300"))
        if not self.key:
            print("[aviso] Falta GROQ_API_KEY: el proveedor groq respondera con un error claro.")

    def _chat(self, messages) -> str:
        body = json.dumps({
            "model": self.model, "messages": messages,
            "temperature": 0.4, "max_tokens": 3000,
        }).encode("utf-8")
        req = urllib.request.Request(URL, data=body, headers={
            "Authorization": f"Bearer {self.key}",
            "Content-Type": "application/json",
            "User-Agent": "DecoMind-AI/0.3",
        })
        with urllib.request.urlopen(req, timeout=40) as resp:
            data = json.loads(resp.read().decode("utf-8"))
        return data["choices"][0]["message"]["content"]

    def generate(self, req: ApiRequest) -> ApiReply:
        if not self.key:
            return ApiReply("Falta la clave de Groq. Define GROQ_API_KEY en el servidor (ver README).")
        user = (f"Modo: {req.mode}\nNivel: {req.level or '(sin nombre)'}\n"
                f"Instrucciones del modo: {req.context}\nPeticion del usuario: {req.prompt}")
        messages = [
            {"role": "system", "content": SYSTEM.replace("{max_objs}", str(self.max_objs))},
            {"role": "user", "content": user},
        ]
        try:
            text = self._chat(messages)
        except urllib.error.HTTPError as e:
            if e.code == 429:
                return ApiReply("Groq esta limitando las peticiones (429). Espera un minuto e intenta de nuevo.")
            if e.code in (401, 403):
                return ApiReply("Groq rechazo la clave (401/403). Revisa GROQ_API_KEY.")
            return ApiReply(f"Groq devolvio un error HTTP {e.code}.")
        except (urllib.error.URLError, TimeoutError):
            return ApiReply("No se pudo contactar con Groq. Revisa la conexion a internet del servidor.")

        try:
            data = extract_json(text)
        except ValueError:
            return ApiReply("La IA no devolvio un formato valido. Intenta de nuevo con otra redaccion.")

        msg = str(data.get("message", "")).strip() or "Listo."
        if req.mode in NO_OBJECT_MODES:
            return ApiReply(msg)
        objs, dropped = build_objects(data.get("objects"), req.mode, self.max_objs)
        if dropped:
            msg += f" (Se descartaron {dropped} objetos no validos.)"
        if not objs:
            msg += " No se coloco ningun objeto."
        return ApiReply(msg, objs)


register_provider("groq", GroqProvider)
