"""Protocolo de texto plano entre el mod y el servidor.

Peticion (cuerpo POST), una clave por linea:
    MODE:<id del modo>        (layout, deco, layout_deco, triggers, ideas, ...)
    CONTEXT:<preprompt interno del modo>
    LEVEL:<nombre del nivel>
    PROMPT:<texto del usuario>
(Tambien se acepta el formato antiguo: linea 1 = modo, resto = prompt.)

Respuesta, una clave por linea:
    MSG:<texto para el panel de resultado>
    USED:<peticiones usadas hoy>
    LIMIT:<limite diario>
    OBJ:<objetos en formato GD "1,<id>,2,<x>,3,<y>;", opcional>
"""
from dataclasses import dataclass


@dataclass
class ApiRequest:
    mode: str = "deco"
    prompt: str = ""
    context: str = ""
    level: str = ""


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
