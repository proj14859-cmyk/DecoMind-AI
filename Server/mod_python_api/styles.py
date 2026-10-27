"""Estilos de decoracion para la IA ("tarjetas de estilo").

La IA ya sabe en general que es "glow", "retro" o "espacial", pero no sabe como se
traduce eso en Geometry Dash: que colores, que familias de objetos, como se capa la
escena y que triggers usar. Cada tarjeta se lo dice de forma corta y concreta.

Como funciona:
  1. `detect_styles(prompt)`  busca estilos en el texto del usuario (sin acentos, por
     palabras clave). Hasta 2 a la vez ("espacial con glow").
  2. `style_block(prompt, mode, catalog)` devuelve el texto que se anade al prompt del
     sistema: paleta (con RGB listos para triggers de color), reglas de composicion,
     familias de objetos del catalogo que encajan, plantillas de composicion y triggers.
  3. Puedes anadir tus propios estilos como JSON en Server/data/styles_user/ (ver
     ESTILOS.md). Si el id coincide con uno incluido, el tuyo lo reemplaza.

Los estilos incluidos son interpretaciones libres, no copias de ningun nivel concreto.
"""
import glob
import json
import os
import re
import unicodedata

from .catalog import DATA_DIR

USER_STYLES_DIR = os.path.join(DATA_DIR, "styles_user")
# Modos en los que tiene sentido aplicar un estilo visual.
STYLE_MODES = {"deco", "layout_deco", "block_design", "transiciones", "efectos"}
MAX_STYLES = 2
MAX_CHARS = 2200

BUILTIN = [
    {
        "id": "glow",
        "name": "Glow / Neon",
        "aliases": ["glow", "neon", "resplandor", "cyberpunk"],
        "mood": "oscuro y limpio, con contornos que brillan",
        "palette": {"fondo": "#04040f", "principal": "#18f0ff", "acento": "#ff2bd6", "detalle": "#ffffff"},
        "reglas": [
            "Fondo muy oscuro y casi vacio; el brillo viene de pocos elementos bien contrastados.",
            "Maximo 2 colores vivos por escena (principal + acento); el blanco solo en detalles pequenos.",
            "Formas simples y redondeadas: contornos, circulos, lineas; evita rellenos grandes.",
            "Repite el mismo motivo a 2 o 3 tamanos (scale 0.5, 1, 1.6) para dar profundidad.",
        ],
        "familias": ["glow", "outline", "circle", "ball", "line", "arrow", "star"],
        "composicion": [
            "linea o contorno principal a lo largo de la ruta, a unos 2 bloques de los bordes",
            "3 circulos concentricos (scale 1, 1.6, 2.2) en el fondo cada ~300 unidades en x",
            "destellos pequenos (scale 0.3-0.5) dispersos sobre el fondo",
        ],
        "triggers": [
            "Pulse del color acento al ritmo de la musica sobre los objetos de contorno",
            "Alpha suave (35=0.6 a 1) en el fondo para que 'respire'",
        ],
        "evitar": ["rellenos grandes opacos", "mas de 3 colores", "objetos pegados a la ruta de salto"],
    },
    {
        "id": "retro",
        "name": "Retro / Old school",
        "aliases": ["retro", "old school", "oldschool", "clasico", "clasica", "antiguo", "pixel", "pixelado", "8 bit", "8bit"],
        "mood": "plano, colorido y simple, como los primeros niveles de GD",
        "palette": {"fondo": "#1b3cff", "principal": "#2fff5e", "acento": "#ffe14a", "detalle": "#000000"},
        "reglas": [
            "Colores planos y saturados, sin degradados; contornos oscuros.",
            "Bloques grandes y modulares con un patron repetido; mucha simetria y alineacion a la cuadricula (multiplos de 30).",
            "Poca decoracion: marcos, pilares y algun detalle repetido cada bloque.",
            "Sin efectos complejos: como mucho un Color o Pulse simple.",
        ],
        "familias": ["pixelart", "square", "outline", "plank", "chain", "saw"],
        "composicion": [
            "pilares decorativos a ambos lados de la ruta, cada 120 unidades en x",
            "fila de detalles pequenos repetidos al mismo nivel y (cuadricula de 30)",
            "marcos o recuadros del fondo alineados al suelo",
        ],
        "triggers": ["Color simple al cambiar de seccion (sin degradados)", "Pulse corto en el golpe fuerte de la musica"],
        "evitar": ["glows", "mezclar muchos estilos", "posiciones fuera de la cuadricula de 30"],
    },
    {
        "id": "infierno",
        "name": "Infierno / Nine Circles (inspirado)",
        "aliases": ["nine circles", "9 circles", "nine circle", "infierno", "inferno", "hell", "demonio", "demoniaco", "demon", "dante", "lava"],
        "mood": "opresivo y ardiente: negros, rojos y naranjas, con humo y cadenas",
        "palette": {"fondo": "#0a0204", "principal": "#c4120f", "acento": "#ff7a1a", "detalle": "#ffd9a0"},
        "reglas": [
            "Fondo casi negro con rojo oscuro; el naranja solo en el foco (lo que debe llamar la atencion).",
            "Siluetas dentadas y verticales: espinas, columnas, cadenas colgando desde arriba.",
            "Capas: fondo muy oscuro y grande, medio con columnas, frente con detalles pequenos.",
            "Ritmo agresivo: mas densidad en los climax, aire en los respiros.",
        ],
        "familias": ["thorn", "chain", "spike", "d_", "saw", "gear", "cloud", "smoke"],
        "composicion": [
            "cadenas colgando desde la parte alta, una cada 150 unidades en x con largo variable",
            "columnas oscuras en el fondo (scale 2-3) con una espina o detalle encima",
            "brasas pequenas (scale 0.3) naranjas que suben desde el suelo",
        ],
        "triggers": [
            "Pulse rojo-naranja en cada golpe fuerte",
            "Move lento y continuo de la capa de fondo para sensacion de humo",
        ],
        "evitar": ["colores frios", "formas redondeadas amables", "decoracion que tape los picos reales"],
    },
    {
        "id": "espacial",
        "name": "Espacial",
        "aliases": ["espacial", "espacio", "space", "galaxia", "cosmos", "cosmico", "estrellas", "planeta", "planetas", "nebulosa", "universo"],
        "mood": "profundo y tranquilo, con muchas capas y puntos de luz",
        "palette": {"fondo": "#050818", "principal": "#4b2fd6", "acento": "#35e0ff", "detalle": "#ffffff"},
        "reglas": [
            "Fondo azul noche o violeta muy oscuro; los colores claros solo en estrellas y detalles.",
            "Profundidad por capas: estrellas diminutas al fondo, planetas medianos, y un cuerpo grande y tenue.",
            "Las estrellas se dispersan de forma irregular (no en cuadricula) y con tamanos distintos.",
            "Los planetas son circulos grandes con un anillo o un detalle; usa rotacion leve (rot) para variar.",
        ],
        "familias": ["star", "circle", "ball", "gear", "glow", "cloud", "arrow"],
        "composicion": [
            "estrellas (scale 0.2-0.5) cada ~40-80 unidades en x, y aleatoria entre 150 y 450",
            "1 planeta grande (scale 2.5-3.5) cada ~600 unidades, algo desplazado hacia arriba",
            "1-2 anillos o circulos concentricos alrededor de cada planeta (scale 1.4 y 1.8)",
        ],
        "triggers": [
            "Move muy lento (28=-20, 10=10) de los grupos de estrellas para parallax",
            "Pulse suave del color acento en los planetas al ritmo",
        ],
        "evitar": ["colores calidos dominantes", "objetos grandes cerca de la ruta", "cuadriculas regulares de estrellas"],
    },
    {
        "id": "ciudad",
        "name": "Ciudad",
        "aliases": ["ciudad", "city", "urbano", "urbana", "edificio", "edificios", "rascacielos", "skyline", "noche urbana"],
        "mood": "siluetas de edificios por capas con luces de ventanas",
        "palette": {"fondo": "#0b1030", "principal": "#1c2a6b", "acento": "#ffc94a", "detalle": "#7fe0ff"},
        "reglas": [
            "Cielo en degradado oscuro (con un Color trigger); los edificios son siluetas de azul oscuro.",
            "Tres capas: edificios lejanos (pequenos y muy oscuros), medios, y cercanos (grandes) con ventanas.",
            "Las ventanas encendidas son puntos pequenos amarillos, en patron irregular.",
            "Alturas variadas: no repitas el mismo edificio dos veces seguidas.",
        ],
        "familias": ["square", "plank", "outline", "pixelart", "d_", "glow", "line"],
        "composicion": [
            "edificios como columnas de bloques decorativos de alturas distintas (3 a 9 bloques)",
            "ventanas: objetos pequenos (scale 0.3) en filas irregulares sobre cada edificio",
            "un cartel o luz de neon (acento) en 1 de cada 3 edificios",
        ],
        "triggers": ["Color suave de cielo al cambiar de seccion", "Alpha o Pulse corto en las ventanas para parpadear"],
        "evitar": ["edificios identicos consecutivos", "detalles claros en los edificios lejanos", "tapar la ruta de salto"],
    },
]


# ---------------------------------------------------------------- utilidades
def _norm(text: str) -> str:
    text = unicodedata.normalize("NFKD", text.lower())
    text = "".join(c for c in text if not unicodedata.combining(c))
    return " " + re.sub(r"[^a-z0-9.]+", " ", text).strip() + " "


def _rgb(hexcolor: str):
    h = hexcolor.lstrip("#")
    return int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)


def load_styles():
    """Estilos incluidos + los del usuario (data/styles_user/*.json)."""
    styles = {s["id"]: s for s in BUILTIN}
    for path in sorted(glob.glob(os.path.join(USER_STYLES_DIR, "*.json"))):
        try:
            with open(path, encoding="utf-8") as fh:
                data = json.load(fh)
            for s in (data if isinstance(data, list) else [data]):
                if isinstance(s, dict) and s.get("id") and s.get("aliases"):
                    styles[str(s["id"])] = s
        except (OSError, ValueError) as exc:
            print(f"[aviso] estilo de usuario ignorado ({os.path.basename(path)}): {exc}")
    return list(styles.values())


def detect_styles(prompt: str, styles=None, limit: int = MAX_STYLES):
    """Devuelve hasta `limit` estilos cuyas palabras clave aparecen en el prompt.
    Mas coincidencias primero; a igualdad, el que el usuario nombra antes."""
    styles = styles if styles is not None else load_styles()
    text = _norm(prompt)
    scored = []
    for order, s in enumerate(styles):
        positions = [text.find(_norm(a)) for a in s.get("aliases", [])]
        positions = [p for p in positions if p >= 0]
        if positions:
            scored.append((-len(positions), min(positions), order, s))
    scored.sort(key=lambda t: t[:3])
    return [t[3] for t in scored[:limit]]


def _families_for(style, catalog):
    """Familias deco del catalogo del juego que encajan con el estilo (por nombre de sprite)."""
    if catalog is None or not getattr(catalog, "game_loaded", False):
        return ""
    wanted = [w.lower() for w in style.get("familias", [])]
    fam = {}
    for oid, e in catalog.entries.items():
        if e["cat"] != "deco":
            continue
        f = e["family"]
        if any(w in f.lower() for w in wanted):
            fam.setdefault(f, []).append(oid)
    if not fam:
        return ""
    items = sorted(fam.items(), key=lambda kv: -len(kv[1]))[:8]
    return "; ".join(f"{k}: {','.join(map(str, sorted(v)[:4]))}" for k, v in items)


def _render(style, catalog) -> str:
    pal = style.get("palette", {})
    pal_txt = ", ".join(
        f"{k}={v} (R,G,B = {'%d,%d,%d' % _rgb(v)})" for k, v in pal.items() if re.fullmatch(r"#[0-9a-fA-F]{6}", str(v))
    )
    lines = [f"ESTILO '{style.get('name', style['id'])}': {style.get('mood', '')}",
             f"Paleta: {pal_txt}"]
    if style.get("reglas"):
        lines.append("Reglas: " + " ".join(f"({i + 1}) {r}" for i, r in enumerate(style["reglas"])))
    fam = _families_for(style, catalog)
    if fam:
        lines.append("IDs deco que encajan con este estilo (familia: ids): " + fam)
    if style.get("composicion"):
        lines.append("Plantillas de composicion (relativas a la zona; adaptalas): " + " | ".join(style["composicion"]))
    if style.get("triggers"):
        lines.append("Triggers recomendados (solo si el modo los permite): " + " | ".join(style["triggers"]))
    if style.get("evitar"):
        lines.append("Evitar: " + "; ".join(style["evitar"]))
    return "\n".join(lines)


def style_block(prompt: str, mode: str, catalog=None) -> str:
    """Texto de estilo para anadir al prompt del sistema ('' si no aplica)."""
    if mode not in STYLE_MODES:
        return ""
    found = detect_styles(prompt)
    if not found:
        return ""
    head = ("ESTILO PEDIDO. Aplica estas guias de estilo con coherencia (misma paleta y mismo "
            "lenguaje de formas en toda la escena). Si hay dos estilos, mezclalos: el primero "
            "manda en la paleta y el segundo en los detalles.\n")
    text = head + "\n\n".join(_render(s, catalog) for s in found)
    return text[:MAX_CHARS]
