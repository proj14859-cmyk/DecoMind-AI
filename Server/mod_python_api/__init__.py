"""mod-python-api: capa minima entre el mod DecoMind AI (Geode) y la IA.

Flujo:  mod (C++)  --HTTP-->  server.py  -->  protocol.parse_request_body
                                          -->  providers.get_provider().generate()
                                          -->  protocol.format_reply  -->  mod

Solo biblioteca estandar. Para conectar una IA real, anade un proveedor en
`providers.py` (ver `StubProvider` y la plantilla `TemplateProvider`).
"""
from .protocol import ApiRequest, ApiReply, parse_request_body, format_reply
from .providers import Provider, StubProvider, get_provider, register_provider
from .quota import DailyQuota

__all__ = [
    "ApiRequest", "ApiReply", "parse_request_body", "format_reply",
    "Provider", "StubProvider", "get_provider", "register_provider", "DailyQuota",
]
__version__ = "0.1.0"
