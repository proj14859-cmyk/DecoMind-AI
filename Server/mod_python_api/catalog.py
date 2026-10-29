"""Catalogo de objetos de GD 2.2 para DecoMind AI (v0.3.7-alpha).

Fuentes (se combinan):
  1. data/catalog_curated.json  - IDs oficiales de uso comun (bloques, outlines,
                                  rellenos, glow, detalles, portales, triggers).
  2. data/catalog_game.txt      - catalogo COMPLETO del juego (lineas "id|frame"),
                                  que el mod sube al servidor con el comando /catalogo.

Cada objeto tiene una categoria:
  solid, hazard, portal, speed, pad, orb, trigger, deco
Las categorias que afectan al gameplay (hazard, portal, speed, pad, orb)
solo se permiten en los modos de layout. Los modos de decoracion y estructuracion
pueden usar `deco` y `trigger` para decorar estructuras y animar entradas/salidas.
"""
import json
import os
import re

DATA_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data")
GAMEPLAY_CATS = {"solid", "hazard", "portal", "speed", "pad", "orb"}

# Categorias que cada modo puede colocar en v0.3.7-alpha.
MODE_CATS = {
    "layout":       {"solid", "hazard", "portal", "speed", "pad", "orb"},
    "layout_deco":  {"solid", "hazard", "portal", "speed", "pad", "orb", "deco", "trigger"},
    "block_design": {"solid", "deco", "trigger"},
    "deco":         {"deco", "trigger"},
    "triggers":     {"trigger"},
    "transiciones": {"trigger", "deco"},
    "efectos":      {"trigger", "deco"},
    "estructurar":  {"deco", "trigger"},
    "ideas":        set(),
}

# Copia de seguridad de data/catalog_curated.json: si el archivo no existe en disco,
# se genera automaticamente para que el servidor en Termux no muestre avisos.
DEFAULT_CURATED = [
    (1, "solid", "bloque basico"),
    (8, "hazard", "pico"),
    (39, "hazard", "pico pequeno"),
    (103, "hazard", "pico mini"),
    (392, "hazard", "pico minimo"),
    (9, "hazard", "pinchos de suelo"),
    (12, "portal", "portal cubo"),
    (13, "portal", "portal nave"),
    (47, "portal", "portal bola"),
    (111, "portal", "portal ufo"),
    (660, "portal", "portal wave"),
    (745, "portal", "portal robot"),
    (1331, "portal", "portal spider"),
    (10, "portal", "portal gravedad normal"),
    (11, "portal", "portal gravedad invertida"),
    (45, "portal", "portal espejo activar"),
    (46, "portal", "portal espejo desactivar"),
    (99, "portal", "portal tamano normal"),
    (101, "portal", "portal tamano mini"),
    (286, "portal", "portal dual"),
    (287, "portal", "portal dual desactivar"),
    (200, "speed", "velocidad 0.5x"),
    (201, "speed", "velocidad 1x"),
    (202, "speed", "velocidad 2x"),
    (203, "speed", "velocidad 3x"),
    (1334, "speed", "velocidad 4x"),
    (35, "pad", "pad amarillo"),
    (67, "pad", "pad azul (gravedad)"),
    (140, "pad", "pad rosa"),
    (1332, "pad", "pad rojo"),
    (36, "orb", "orbe amarillo"),
    (84, "orb", "orbe azul (gravedad)"),
    (141, "orb", "orbe rosa"),
    (1022, "orb", "orbe verde"),
    (1330, "orb", "orbe negro"),
    (1333, "orb", "orbe rojo"),
    (1704, "orb", "orbe dash verde"),
    (1751, "orb", "orbe dash rosa"),
    (899, "trigger", "trigger Color"),
    (901, "trigger", "trigger Move"),
    (1006, "trigger", "trigger Pulse"),
    (1007, "trigger", "trigger Alpha"),
    (1049, "trigger", "trigger Toggle"),
    (1346, "trigger", "trigger Rotate"),
    (2067, "trigger", "trigger Scale"),
    (1347, "trigger", "trigger Follow"),
    (1268, "trigger", "trigger Spawn"),
    (1616, "trigger", "trigger Stop"),
    # --- IDs adicionales de bloques y decoracion ---
    *[(i, "solid", "bloque estructural") for i in (2, 3, 4, 5, 6, 7, 62, 63, 64, 65, 66, 83, 90, 91, 144, 145, 160, 161, 192)],
    *[(i, "deco", "borde") for i in (40, 41, 467, 468)],
    *[(i, "deco", "relleno sin colision") for i in (211, 503, 918)],
    *[(i, "deco", "halo / esquina de luz") for i in (504, 505, 1001, 1002)],
    *[(i, "deco", "detalle geometrico / tecnologico") for i in (579, 580, 725, 919, 1140, 1249, 1250, 1585, 1587, 1705, 1706, 1707, 1755, 1888)],
    (1933, "portal", "portal swing"),
]

_FAMILY_RE = re.compile(r"^(.*?)(?:_\d+)+$")
_HAZARD_RE = re.compile(r"(spike|saw|blade|thorn|pit|monster|hazard)", re.I)
_SPECIAL_RE = re.compile(r"(portal|pad|ring|orb|boost|speed|coin|trigger|edit_|button|btn|gj_|player|icon)", re.I)
_SOLID_RE = re.compile(r"^(square|plank|slab|block|tile|ground|outline_sq)", re.I)
_DECO_RE = re.compile(r"^(d_|pixelart|blockoutline|deco|glow|particle|bush|chain|cloud)", re.I)


def family_of(frame: str) -> str:
    name = frame[:-4] if frame.lower().endswith(".png") else frame
    m = _FAMILY_RE.match(name)
    return m.group(1) if m else name


def classify_frame(frame: str) -> str:
    """Clasifica un objeto del juego por el nombre de su sprite. 'other' = no se usa."""
    name = family_of(frame)
    if _HAZARD_RE.search(name):
        return "hazard"
    if _SPECIAL_RE.search(name):
        return "other"
    if _DECO_RE.match(name):
        return "deco"
    if _SOLID_RE.match(name):
        return "solid"
    return "other"


class Catalog:
    def __init__(self, data_dir: str = DATA_DIR):
        self.data_dir = data_dir
        self.entries = {}   # id -> {"cat":..., "name":..., "family":...}
        self.game_loaded = False
        self.reload()

    # ------------------------------------------------------------------ carga
    def reload(self):
        self.entries = {}
        self.game_loaded = False
        game = os.path.join(self.data_dir, "catalog_game.txt")
        if os.path.isfile(game):
            self._load_game(open(game, encoding="utf-8", errors="replace").read())
        cur = os.path.join(self.data_dir, "catalog_curated.json")
        if not os.path.isfile(cur):
            try:
                os.makedirs(self.data_dir, exist_ok=True)
                payload = {
                    "entries": [
                        {"id": oid, "cat": cat, "name": name}
                        for oid, cat, name in DEFAULT_CURATED
                    ]
                }
                with open(cur, "w", encoding="utf-8") as fh:
                    json.dump(payload, fh, ensure_ascii=False, indent=2)
            except Exception:
                pass
        if os.path.isfile(cur):
            curated = [(int(e["id"]), e["cat"], e["name"])
                       for e in json.load(open(cur, encoding="utf-8")).get("entries", [])]
        else:
            curated = DEFAULT_CURATED
        for oid, cat, name in curated:
            # lo curado manda sobre la clasificacion automatica
            self.entries[oid] = {"cat": cat, "name": name, "family": name}

    def _load_game(self, text: str):
        for line in text.splitlines():
            oid, _, frame = line.partition("|")
            if not oid.strip().isdigit() or not frame.strip():
                continue
            cat = classify_frame(frame.strip())
            if cat == "other":
                continue
            self.entries[int(oid)] = {"cat": cat, "name": frame.strip(), "family": family_of(frame.strip())}
        self.game_loaded = bool(self.entries)

    def save_game_catalog(self, text: str) -> int:
        """Guarda el catalogo subido por el mod y lo recarga. Devuelve cuantas lineas validas tenia."""
        valid = [ln.strip() for ln in text.splitlines()
                 if "|" in ln and ln.split("|", 1)[0].strip().isdigit()]
        os.makedirs(self.data_dir, exist_ok=True)
        with open(os.path.join(self.data_dir, "catalog_game.txt"), "w", encoding="utf-8") as f:
            f.write("\n".join(valid) + "\n")
        self.reload()
        return len(valid)

    # ---------------------------------------------------------------- consultas
    def allowed_cats(self, mode: str):
        return MODE_CATS.get(mode, set())

    def is_allowed(self, mode: str, oid: int) -> bool:
        e = self.entries.get(oid)
        return bool(e) and e["cat"] in self.allowed_cats(mode)

    def has_cat(self, cats) -> bool:
        return any(e["cat"] in cats for e in self.entries.values())

    def describe(self, mode: str, max_chars: int = 5500) -> str:
        """Texto compacto con los IDs que la IA puede usar en este modo."""
        cats = self.allowed_cats(mode)
        lines = []
        for cat in ("solid", "hazard", "portal", "speed", "pad", "orb", "trigger", "deco"):
            if cat not in cats:
                continue
            items = sorted((i, e) for i, e in self.entries.items() if e["cat"] == cat)
            if not items:
                continue
            if cat in ("solid", "deco") and self.game_loaded:
                fam = {}
                for i, e in items:
                    fam.setdefault(e["family"], []).append(i)
                body = "; ".join(f"{k}: {','.join(map(str, v[:6]))}" for k, v in sorted(fam.items()))
            else:
                body = "; ".join(f"{i}={e['name']}" for i, e in items)
            lines.append(f"[{cat}] {body}")
        text = "\n".join(lines)
        return text[:max_chars]
