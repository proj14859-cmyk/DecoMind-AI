"""Protocolo de texto plano entre el mod DecoMind AI (v0.3.4-alpha) y el servidor.

Peticion (cuerpo POST), una clave por linea:
    MODE:<id del modo>                 (layout, deco, layout_deco, block_design, triggers, ...)
    CONTEXT:<preprompt interno del modo>
    LEVEL:<nombre del nivel>
    LEVEL_STATS:<total_objects, camera, zoom>
    SELECTION:<resumen y coordenadas exactas de objetos seleccionados, o 'none'>
    VIEWPORT:<resumen de objetos visibles en la camara actual>
    USED_GROUPS:<rangos de Group IDs ocupados en el nivel>
    FREE_GROUPS:<siguiente Group ID libre y lista de sugeridos>
    USED_COLORS:<rangos de Color IDs ocupados y paleta activa>
    FREE_COLORS:<siguiente Color ID libre y lista de sugeridos>
    PROMPT:<texto del usuario>
(Tambien se acepta el formato antiguo: linea 1 = modo, resto = prompt.)

Respuesta, una clave por linea:
    MSG:<texto para el panel de resultado>
    USED:<peticiones usadas hoy>
    LIMIT:<limite diario>
    OBJ:<objetos en formato GD "1,<id>,2,<x>,3,<y>,...;", opcional>
"""
from dataclasses import dataclass


@dataclass
class ApiRequest:
    mode: str = "deco"
    prompt: str = ""
    context: str = ""
    level: str = ""
    level_stats: str = ""
    selection: str = ""
    viewport: str = ""
    used_groups: str = ""
    free_groups: str = ""
    used_colors: str = ""
    free_colors: str = ""

    def format_editor_perception(self) -> str:
        """Devuelve un resumen estructurado de la percepcion del nivel para la IA."""
        lines = []
        if self.level_stats:
            lines.append(f"- Estadisticas y camara del nivel: {self.level_stats}")
        if self.selection and self.selection != "none":
            lines.append(f"- ESTRUCTURA SELECCIONADA POR EL USUARIO (prioridad maxima): {self.selection}")
        else:
            lines.append("- ESTRUCTURA SELECCIONADA: ninguna (usa la vista de camara VIEWPORT o x=300)")
        if self.viewport:
            lines.append(f"- Objetos visibles en la camara actual (VIEWPORT): {self.viewport}")
        if self.used_groups:
            lines.append(f"- Group IDs OCUPADOS en el nivel (NO pisar salvo que el usuario lo pida): {self.used_groups}")
        if self.free_groups:
            lines.append(f"- Group IDs LIBRES recomendados: {self.free_groups}")
        if self.used_colors:
            lines.append(f"- Color IDs OCUPADOS en el nivel: {self.used_colors}")
        if self.free_colors:
            lines.append(f"- Color IDs LIBRES recomendados: {self.free_colors}")
        return "\n".join(lines) if lines else "- Sin datos adicionales de seleccion enviados por el cliente."


@dataclass
class ApiReply:
    message: str = ""
    objects: str = ""  # cadena de objetos GD; vacio = no tocar el nivel


def parse_request_body(body: str) -> ApiRequest:
    lines = [ln.strip() for ln in body.splitlines() if ln.strip()]
    if not lines:
        return ApiRequest()
    if any(ln.startswith(("MODE:", "PROMPT:")) for ln in lines):
        req = ApiRequest()
        for ln in lines:
            if ln.startswith("MODE:"):
                req.mode = ln[5:].strip()
            elif ln.startswith("CONTEXT:"):
                req.context = ln[8:].strip()
            elif ln.startswith("LEVEL:"):
                req.level = ln[6:].strip()
            elif ln.startswith("LEVEL_STATS:"):
                req.level_stats = ln[12:].strip()
            elif ln.startswith("SELECTION:"):
                req.selection = ln[10:].strip()
            elif ln.startswith("VIEWPORT:"):
                req.viewport = ln[9:].strip()
            elif ln.startswith("USED_GROUPS:"):
                req.used_groups = ln[12:].strip()
            elif ln.startswith("FREE_GROUPS:"):
                req.free_groups = ln[12:].strip()
            elif ln.startswith("USED_COLORS:"):
                req.used_colors = ln[12:].strip()
            elif ln.startswith("FREE_COLORS:"):
                req.free_colors = ln[12:].strip()
            elif ln.startswith("PROMPT:"):
                req.prompt = ln[7:].strip()
        return req
    first, _, rest = body.partition("\n")
    return ApiRequest(mode=first.strip(), prompt=rest.strip())


def format_reply(reply: ApiReply, used: int, limit: int) -> str:
    out = f"MSG:{reply.message.replace(chr(10), ' ')}\nUSED:{used}\nLIMIT:{limit}\n"
    if reply.objects:
        out += f"OBJ:{reply.objects}\n"
    return out
