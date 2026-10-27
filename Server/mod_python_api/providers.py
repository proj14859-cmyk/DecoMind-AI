"""Proveedores de IA para DecoMind AI (v0.3.4-alpha).
Cada proveedor convierte una ApiRequest en una ApiReply leyendo el contexto del
nivel (SELECTION, VIEWPORT, USED_GROUPS, FREE_GROUPS, USED_COLORS, FREE_COLORS).

Para conectar una IA real:
  1. Usa `DECOMIND_PROVIDER=groq` con tu `GROQ_API_KEY` en `Server/.env`, o
  2. Copia `TemplateProvider`, implementa `generate()` y registralo con `register_provider`.
"""
import os
import re
from .protocol import ApiReply, ApiRequest


class Provider:
    name = "base"

    def generate(self, req: ApiRequest) -> ApiReply:  # pragma: no cover - interfaz
        raise NotImplementedError


def _parse_bbox(meta_str: str):
    """Extrae bbox=minX,minY,maxX,maxY de SELECTION o VIEWPORT."""
    m = re.search(r"bbox=(-?\d+),(-?\d+),(-?\d+),(-?\d+)", meta_str or "")
    if not m:
        return None
    x1, y1, x2, y2 = map(int, m.groups())
    return min(x1, x2), min(y1, y2), max(x1, x2), max(y1, y2)


def _parse_next_free(free_str: str, default_val: int) -> int:
    m = re.search(r"next=(\d+)", free_str or "")
    return int(m.group(1)) if m else default_val


def _extract_range_near(prompt: str, keywords, lo_lim: int, hi_lim: int):
    low = prompt.lower()
    for kw in keywords:
        idx = low.find(kw)
        if idx == -1:
            continue
        sub = low[idx:idx + 65]
        nums = [int(n) for n in re.findall(r"\d+", sub) if lo_lim <= int(n) <= hi_lim]
        if nums:
            start = nums[0]
            end = nums[1] if (len(nums) >= 2 and nums[1] >= start) else start
            return start, end
    return None


def _simple_layout(seconds: int, start_x: int = 300, group_id: int = 1, color_id: int = 1) -> str:
    """Layout de prueba con soporte de grupos y colores libres."""
    out, x = [], max(300, start_x)
    for i in range(max(2, seconds // 2)):
        kind = i % 4
        if kind == 0:
            out.append(f"1,8,2,{x},3,15;")
            x += 240
        elif kind == 1:
            out.append(f"1,8,2,{x},3,15;1,8,2,{x + 30},3,15;")
            x += 270
        elif kind == 2:
            out.append(f"1,1,2,{x},3,15,21,{color_id},57,{group_id};1,8,2,{x + 90},3,15;")
            x += 270
        else:
            out.append(
                f"1,1,2,{x},3,15,21,{color_id},57,{group_id};"
                f"1,1,2,{x + 30},3,15,21,{color_id},57,{group_id};"
                f"1,1,2,{x + 30},3,45,21,{color_id},57,{group_id};"
                f"1,1,2,{x + 60},3,15,21,{color_id},57,{group_id};"
                f"1,1,2,{x + 60},3,45,21,{color_id},57,{group_id};"
                f"1,1,2,{x + 60},3,75,21,{color_id},57,{group_id};"
            )
            x += 330
    return "".join(out)


def _contextual_deco_and_triggers(req: ApiRequest):
    """Deco local (sin IA) sobre la seleccion o la camara: relleno, bordes, glow y triggers.

    Devuelve (mensaje, objetos) o None si no hay donde decorar. Mismos IDs que LevelExecutor.hpp.
    """
    box = _parse_bbox(req.selection) or _parse_bbox(req.viewport)
    if not box:
        return None
    x1, y1, x2, y2 = box
    x2 = min(x2, x1 + 240)  # shortcut: tope de 9 columnas, subir si hace falta mas deco
    y2 = min(y2, y1 + 150)
    g = _extract_range_near(req.prompt, ("group id de objetos", "group id", "grupo"), 1, 9999)
    c = _extract_range_near(req.prompt, ("color id", "colores", "canal"), 1, 1105)
    g0 = g[0] if g else _parse_next_free(req.free_groups, 9000)
    c0 = c[0] if c else _parse_next_free(req.free_colors, 900)
    g1 = g[1] if g and g[1] > g[0] else g0 + 1
    c1 = c[1] if c and c[1] > c[0] else c0 + 1
    out = []
    for x in range(x1, x2 + 1, 30):
        for y in range(y1, y2 + 1, 30):
            out.append(f"1,503,2,{x},3,{y},21,{c0},22,{c1},24,-1,25,-2,57,{g0};")
            if y + 30 > y2:
                out.append(f"1,40,2,{x},3,{y},21,{c1},24,1,25,3,57,{g0}.{g1};")
    out.append(f"1,504,2,{(x1 + x2) // 2},3,{(y1 + y2) // 2},32,2,21,{c1},24,-3,25,-10,57,{g1};")
    tx, ty = max(30, x1 - 120), y2 + 90
    out.append(f"1,899,2,{tx},3,{ty},7,24,8,200,9,255,10,0.2,23,{c0},35,1;")
    out.append(f"1,899,2,{tx},3,{ty + 30},7,182,8,72,9,246,10,0.2,23,{c1},35,1;")
    msg = (f"[prueba] Deco local en X={x1}..{x2}, Y={y1}..{y2} con grupos {g0}..{g1} "
           f"y colores {c0}..{c1}. Para una deco real configura un proveedor de IA.")
    return msg, "".join(out)


class StubProvider(Provider):
    """Proveedor de prueba sin IA real."""
    name = "stub"

    def generate(self, req: ApiRequest) -> ApiReply:
        if req.mode == "ideas":
            return ApiReply("[prueba] Idea: alterna secciones calmas y rapidas, y refuerza "
                            "los cambios de ritmo con contraste de color y pulsos suaves.")
        if req.mode in ("layout", "layout_deco"):
            secs = _seconds_from(req.prompt)
            gid = _parse_next_free(req.free_groups, 1)
            cid = _parse_next_free(req.free_colors, 1)
            return ApiReply(f"[prueba] Modo {req.mode}: layout simple de {secs}s para: {req.prompt}",
                            _simple_layout(secs, group_id=gid, color_id=cid))
        res = _contextual_deco_and_triggers(req)
        if res and req.mode != "estructurar":
            return ApiReply(*res)
        return ApiReply(f"[prueba] Modo {req.mode} recibido (proveedor IA pendiente de "
                        f"configurar). Peticion: {req.prompt}")


def _seconds_from(prompt: str) -> int:
    for word in prompt.lower().replace(",", " ").split():
        if word.isdigit():
            return max(5, min(int(word), 120))
    return 20


class TemplateProvider(Provider):
    """Plantilla para una IA real. NO esta registrada; copiala y completala."""
    name = "template"

    def __init__(self):
        self.api_key = os.environ.get("DECOMIND_API_KEY", "")

    def generate(self, req: ApiRequest) -> ApiReply:
        raise NotImplementedError("Implementa la llamada a la IA aqui.")


_REGISTRY = {"stub": StubProvider}


def register_provider(name: str, cls) -> None:
    _REGISTRY[name] = cls


def get_provider(name: str = "") -> Provider:
    from . import groq_provider  # noqa: F401  (registra "groq")
    name = (name or os.environ.get("DECOMIND_PROVIDER", "stub")).strip().lower()
    if name not in _REGISTRY:
        print(f"[aviso] proveedor '{name}' no existe; uso 'stub'. Disponibles: {', '.join(_REGISTRY)}")
        name = "stub"
    return _REGISTRY[name]()
