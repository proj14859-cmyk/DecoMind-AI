#!/usr/bin/env python3
"""
Servidor de prueba para AI Decorator.

Corre en Termux (movil) o en tu PC. El mod hace un POST con el prompt en el
cuerpo y espera como respuesta una cadena de objetos de GD ("1,8,2,300,3,15;...").

Sin dependencias externas (solo libreria estandar de Python 3).

Uso:
    python server.py            # escucha en 0.0.0.0:8765
    python server.py 9000       # otro puerto

Mas adelante, en generate_level(), se puede reemplazar la logica de reglas por
una llamada a un LLM con tool calling.
"""
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8765


def generate_level(prompt: str) -> str:
    """Devuelve objetos en formato GD. Por ahora usa reglas simples.

    Formato: "1,<id>,2,<x>,3,<y>;"   id 1 = bloque, id 8 = pico.
    Un bloque mide 30 unidades; y = 15 es el suelo.
    """
    text = prompt.lower()

    # Duracion aproximada: "20 segundos" -> mas o menos 1 seccion cada 2 s
    seconds = 20
    for word in text.replace(",", " ").split():
        if word.isdigit():
            seconds = max(5, min(int(word), 120))
            break
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


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        prompt = self.rfile.read(length).decode("utf-8", errors="replace")
        print(f"[prompt] {prompt!r}")
        level = generate_level(prompt)
        data = level.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)
        print(f"[ok] {len(data)} bytes enviados")

    def do_GET(self):
        data = b"AI Decorator server OK"
        self.send_response(200)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


if __name__ == "__main__":
    print(f"Escuchando en http://0.0.0.0:{PORT}/generate  (Ctrl+C para salir)")
    HTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
