#!/usr/bin/env python3
"""
Servidor de prueba de DecoMind AI (v0.2).

Corre en Termux o en un PC. Sin dependencias externas.

Peticion del mod (POST):
    linea 1: id del modo (layout, deco, layout_deco, triggers, ...)
    resto  : el texto que escribio el usuario

Respuesta (texto plano, una clave por linea):
    MSG:<texto que se muestra en el panel de resultado>
    USED:<peticiones usadas hoy>
    LIMIT:<limite diario>
    OBJ:<objetos en formato GD, opcional>

Uso:
    python server.py            # puerto 8765
    python server.py 9000       # otro puerto
    DAILY_LIMIT=50 python server.py
"""
import datetime
import os
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
DAILY_LIMIT = int(os.environ.get("DAILY_LIMIT", "20"))

_state = {"day": None, "used": 0}


def take_request() -> bool:
    """Cuenta una peticion del dia. False si ya se alcanzo el limite."""
    today = datetime.date.today().isoformat()
    if _state["day"] != today:
        _state["day"] = today
        _state["used"] = 0
    if _state["used"] >= DAILY_LIMIT:
        return False
    _state["used"] += 1
    return True


def simple_layout(seconds: int) -> str:
    """Layout de prueba. Formato GD: 1,<id>,2,<x>,3,<y>;  (1 = bloque, 8 = pico)."""
    sections = max(2, seconds // 2)
    out = []
    x = 300
    for i in range(sections):
        kind = i % 4
        if kind == 0:
            out.append(f"1,8,2,{x},3,15;")
            x += 240
        elif kind == 1:
            out.append(f"1,8,2,{x},3,15;1,8,2,{x + 30},3,15;")
            x += 270
        elif kind == 2:
            out.append(f"1,1,2,{x},3,15;1,8,2,{x + 90},3,15;")
            x += 270
        else:
            out.append(
                f"1,1,2,{x},3,15;"
                f"1,1,2,{x + 30},3,15;1,1,2,{x + 30},3,45;"
                f"1,1,2,{x + 60},3,15;1,1,2,{x + 60},3,45;1,1,2,{x + 60},3,75;"
            )
            x += 330
    return "".join(out)


def seconds_from(prompt: str) -> int:
    for word in prompt.lower().replace(",", " ").split():
        if word.isdigit():
            return max(5, min(int(word), 120))
    return 20


def handle(mode: str, prompt: str):
    """Devuelve (mensaje, objetos). Aqui se conectara la IA real."""
    if mode in ("layout", "layout_deco"):
        secs = seconds_from(prompt)
        objs = simple_layout(secs)
        msg = f"[prueba] Modo {mode}: layout simple de {secs}s para: {prompt}"
        return msg, objs
    if mode == "ideas":
        return ("[prueba] Idea: alterna secciones calmas y rapidas, y refuerza "
                "los cambios de ritmo con color y pulsos."), ""
    return f"[prueba] Modo {mode} recibido: aun no hay IA conectada. Peticion: {prompt}", ""


class Handler(BaseHTTPRequestHandler):
    def _send(self, text: str):
        data = text.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length).decode("utf-8", errors="replace")
        first, _, rest = body.partition("\n")
        mode, prompt = first.strip(), rest.strip()
        print(f"[peticion] modo={mode!r} prompt={prompt!r}")

        if not take_request():
            reply = (f"MSG:Limite diario alcanzado ({DAILY_LIMIT}). Vuelve manana.\n"
                     f"USED:{_state['used']}\nLIMIT:{DAILY_LIMIT}\n")
            self._send(reply)
            return

        msg, objs = handle(mode, prompt)
        msg = msg.replace("\n", " ")
        reply = f"MSG:{msg}\nUSED:{_state['used']}\nLIMIT:{DAILY_LIMIT}\n"
        if objs:
            reply += f"OBJ:{objs}\n"
        self._send(reply)
        print(f"[ok] usadas hoy: {_state['used']}/{DAILY_LIMIT}")

    def do_GET(self):
        self._send("DecoMind AI server OK")


if __name__ == "__main__":
    print(f"DecoMind AI escuchando en http://0.0.0.0:{PORT}/generate "
          f"(limite diario {DAILY_LIMIT}). Ctrl+C para salir.")
    HTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
