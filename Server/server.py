#!/usr/bin/env python3
"""
Servidor de DecoMind AI (v0.3.4-alpha).

Corre en Termux o en un PC. Sin dependencias externas.

Soporta tanto el formato estructurado (MODE / CONTEXT / LEVEL / PROMPT)
como el formato simple de 2 lineas:
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

from mod_python_api import (ApiReply, DailyQuota, format_reply, get_provider,
                            parse_request_body)
from mod_python_api.catalog import Catalog

def _load_env():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".env")
    if os.path.isfile(path):
        for line in open(path, encoding="utf-8"):
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                k, _, v = line.partition("=")
                os.environ.setdefault(k.strip(), v.strip().strip('"').strip("'"))


_load_env()
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
DAILY_LIMIT = int(os.environ.get("DAILY_LIMIT", "20"))

quota = DailyQuota(DAILY_LIMIT)
provider = get_provider()  # DECOMIND_PROVIDER=stub (por defecto)
catalog = Catalog()
MAX_CATALOG_BYTES = 3_000_000


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
        if self.path.startswith("/catalog"):
            # El mod sube aqui el catalogo completo de objetos del juego (comando /catalogo).
            if length <= 0 or length > MAX_CATALOG_BYTES:
                self._send("MSG:Catalogo rechazado (tamano invalido).\n")
                return
            n = catalog.save_game_catalog(self.rfile.read(length).decode("utf-8", errors="replace"))
            print(f"[catalogo] recibidos {n} objetos del juego")
            self._send(f"MSG:Catalogo recibido: {n} objetos.\nCOUNT:{n}\n")
            return
        body = self.rfile.read(length).decode("utf-8", errors="replace")
        req = parse_request_body(body)
        print(f"[peticion] modo={req.mode!r} prompt={req.prompt!r}")

        if not quota.take():
            self._send(format_reply(
                ApiReply(f"Limite diario alcanzado ({DAILY_LIMIT}). Vuelve manana."),
                quota.used, DAILY_LIMIT))
            return
        try:
            reply = provider.generate(req)
        except Exception as exc:  # el mod siempre recibe una respuesta valida
            reply = ApiReply(f"Error del proveedor de IA: {exc}")
        self._send(format_reply(reply, quota.used, DAILY_LIMIT))
        print(f"[ok] usadas hoy: {quota.used}/{DAILY_LIMIT}")

    def do_GET(self):
        info = "catalogo del juego cargado" if catalog.game_loaded else "sin catalogo del juego (usa /catalogo)"
        self._send(format_reply(ApiReply(f"DecoMind AI server OK - {info}"), quota.used, DAILY_LIMIT))


if __name__ == "__main__":
    print(f"DecoMind AI escuchando en http://0.0.0.0:{PORT}/generate "
          f"(proveedor {provider.name}, limite diario {DAILY_LIMIT}). Ctrl+C para salir.")
    HTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
