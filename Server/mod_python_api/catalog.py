"""Catalogo de objetos de GD para la IA.

Fuentes (se combinan):
  1. data/catalog_curated.json  - IDs de uso comun (editable a mano).
  2. data/catalog_game.txt      - catalogo COMPLETO del juego (lineas "id|frame"),
                                  que el mod sube al servidor con el comando /catalogo.

Cada objeto tiene una categoria:
  solid, hazard, portal, speed, pad, orb, trigger, deco
Las categorias que afectan al gameplay (solid, hazard, portal, speed, pad, orb)
solo se permiten en los modos de layout. Los modos de decoracion solo pueden
usar `deco` (y `trigger` en triggers / transiciones / efectos).
"""
import json
import os
import re

DATA_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data")
GAMEPLAY_CATS = {"solid", "hazard", "portal", "speed", "pad", "orb"}

# Categorias que cada modo puede colocar.
MODE_CATS = {
    "layout":       {"solid", "hazard", "portal", "speed", "pad", "orb"},
    "layout_deco":  {"solid", "hazard", "portal", "speed", "pad", "orb", "deco"},
    "block_design": {"solid", "deco"},
    "deco":         {"deco"},
    "triggers":     {"trigger"},
    "transiciones": {"trigger", "deco"},
    "efectos":      {"trigger", "deco"},
    "estructurar":  set(),
    "ideas":        set(),
}

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
        if os.path.isfile(cur):
            for e in json.load(open(cur, encoding="utf-8")).get("entries", []):
                # lo curado manda sobre la clasificacion automatica
                self.entries[int(e["id"])] = {"cat": e["cat"], "name": e["name"], "family": e["name"]}

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

    def describe(self, mode: str, max_chars: int = 5000) -> str:
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
