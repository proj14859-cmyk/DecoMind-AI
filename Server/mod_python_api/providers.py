"""Proveedores de IA. Cada proveedor convierte una ApiRequest en una ApiReply.

Para conectar una IA real:
  1. Copia `TemplateProvider`, implementa `generate()` (llamada HTTP a la IA).
  2. Registralo con `register_provider("nombre", MiProveedor)`.
  3. Arranca el servidor con  DECOMIND_PROVIDER=nombre python server.py
Las claves de API se leen de variables de entorno, nunca del codigo.
"""
import os
from .protocol import ApiReply, ApiRequest


class Provider:
    name = "base"

    def generate(self, req: ApiRequest) -> ApiReply:  # pragma: no cover - interfaz
        raise NotImplementedError


def _simple_layout(seconds: int) -> str:
    """Layout de prueba. Formato GD: 1,<id>,2,<x>,3,<y>;  (1 = bloque, 8 = pico)."""
    out, x = [], 300
    for i in range(max(2, seconds // 2)):
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
            out.append(f"1,1,2,{x},3,15;1,1,2,{x + 30},3,15;1,1,2,{x + 30},3,45;"
                       f"1,1,2,{x + 60},3,15;1,1,2,{x + 60},3,45;1,1,2,{x + 60},3,75;")
            x += 330
    return "".join(out)


def _seconds_from(prompt: str) -> int:
    for word in prompt.lower().replace(",", " ").split():
        if word.isdigit():
            return max(5, min(int(word), 120))
    return 20


class StubProvider(Provider):
    """Proveedor de prueba sin IA real (es el comportamiento de v0.3)."""
    name = "stub"

    def generate(self, req: ApiRequest) -> ApiReply:
        if req.mode in ("layout", "layout_deco"):
            secs = _seconds_from(req.prompt)
            return ApiReply(
                f"[prueba] Modo {req.mode}: layout simple de {secs}s generado para: {req.prompt}",
                _simple_layout(secs))
        if req.mode == "ideas":
            return ApiReply("[prueba] Idea: alterna secciones calmas y rapidas, y refuerza "
                            "los cambios de ritmo con contraste de color y pulsos suaves.")
        return ApiReply(f"[prueba] Modo {req.mode} recibido (proveedor IA pendiente de "
                        f"configurar). Peticion: {req.prompt}")


class TemplateProvider(Provider):
    """Plantilla para una IA real. NO esta registrada; copiala y completala."""
    name = "template"

    def __init__(self):
        self.api_key = os.environ.get("DECOMIND_API_KEY", "")

    def generate(self, req: ApiRequest) -> ApiReply:
        # 1) construir el prompt:  req.context (preprompt del modo) + req.prompt
        # 2) llamar a la IA (urllib.request) con self.api_key
        # 3) validar/convertir la respuesta a objetos GD y devolver ApiReply(msg, objs)
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
