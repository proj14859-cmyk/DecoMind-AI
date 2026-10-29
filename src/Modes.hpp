#pragma once
// ===========================================================================
// DecoMind AI v0.3.6-alpha - Capa de Gestion de Modos y Estado
//
// Define los 9 modos de construccion y decoracion con sus categorias,
// desplazamientos cromaticos vecinos (±28 grados de matiz segun el PDF),
// etiquetas de capacidades ("PUEDE USAR" con check y equis) y preprompts.
// ===========================================================================
#include <Geode/Geode.hpp>
#include <algorithm>
#include <cctype>
#include <string>

using namespace geode::prelude;

namespace decomind {

enum class ModeIconType {
    Estructurar = 0,
    Layout,
    Deco,
    LayoutDeco,
    BlockDesign,
    Triggers,
    Transiciones,
    Efectos,
    Ideas
};

struct ModeDefinition {
    char const* id;
    char const* title;          // Titulo en mayusculas en las tarjetas
    char const* name;           // Nombre corto
    char const* categoryTag;    // Etiqueta derecha en la pestana Informacion (Orden, Jugable, Visual, etc.)
    char const* visibleLine1;   // Linea 1 de descripcion corta en la tarjeta
    char const* visibleLine2;   // Linea 2 de descripcion corta en la tarjeta
    char const* userGuide;      // Explicacion en la pestana Informacion / Modos
    char const* canUse[3];      // 3 pildoras con check (✓) en "PUEDE USAR"
    char const* cannotUse[2];   // 2 pildoras con equis (✕) en "PUEDE USAR"
    char const* examplePrompt;  // Ejemplo de prueba interactivo
    char const* internalPrompt; // Preprompt tecnico enviado al servidor / proveedor IA
    char const* spriteName;     // Icono PNG blanco recoloreable en resources/
    float hueOffsetDeg;         // Desplazamiento de matiz vecino (-28..+28 grados)
    ccColor3B borderColor;      // Color de respaldo
    ccColor3B bgColor;          // Color de fondo de respaldo
    ModeIconType iconType;
};

static const ModeDefinition MODES[9] = {
    {
        "estructurar",
        "ESTRUCTURAR",
        "Estructurar",
        "Orden",
        "Organiza y estructura tu nivel de",
        "forma inteligente.",
        "Ordena tus bloques en grupos limpios, asigna canales de color libres y prepara la base logica del nivel respetando las estructuras seleccionadas.",
        {"Objetos decorativos", "Grupos y capas libres", "Triggers de organizacion"},
        {"Picos con colision", "Portales y velocidades"},
        "Estructura estos bloques en 3 grupos y prepara bordes neon",
        "Eres el modulo ESTRUCTURAR de Geometry Dash. Organiza las estructuras seleccionadas en grupos libres, asigna capas Z coherentes y anade contornos o triggers de organizacion sin alterar la jugabilidad.",
        "mode-estructurar.png"_spr,
        -28.f,
        ccc3(0x9b, 0x5c, 0xf6),
        ccc3(0x18, 0x10, 0x28),
        ModeIconType::Estructurar
    },
    {
        "layout",
        "LAYOUT",
        "Layout",
        "Jugable",
        "Crea la estructura y la jugabilidad",
        "del nivel.",
        "Construye plataformas, saltos calibrados, picos, orbes, pads y cambios de modo con ritmo jugable y distancias superables.",
        {"Bloques y plataformas", "Picos y obstaculos", "Portales, orbes y pads"},
        {"Sobrecarga decorativa", "Efectos distractores"},
        "Genera una estructura pequena con un salto de tres pinchos",
        "Eres un creador experto de layouts en Geometry Dash. Genera gameplay divertido, leible y balanceado usando bloques base, picos, pads, orbes y portales con alturas y distancias fisicamente superables.",
        "mode-layout.png"_spr,
        -21.f,
        ccc3(0x88, 0x64, 0xf8),
        ccc3(0x16, 0x11, 0x29),
        ModeIconType::Layout
    },
    {
        "deco",
        "DECO",
        "Deco",
        "Visual",
        "Anade y mejora la decoracion del",
        "nivel.",
        "Decora tus estructuras existentes con detalles visuales, atmosfera y contraste, respetando las colisiones. Nunca cambia la jugabilidad.",
        {"Objetos decorativos", "Triggers de color y animacion", "Respeta colisiones"},
        {"Picos y bloques", "Portales, orbes y velocidades"},
        "Mejora la deco de esta parte con estilo neon azul y detalles brillantes",
        "Eres un decorador profesional de Geometry Dash. Anade detalles visuales, glow, conectores y triggers de color/animacion sobre los bloques existentes respetando las hitboxes.",
        "mode-deco.png"_spr,
        -14.f,
        ccc3(0xa8, 0x55, 0xf7),
        ccc3(0x1a, 0x10, 0x2a),
        ModeIconType::Deco
    },
    {
        "layout_deco",
        "LAYOUT + DECO",
        "Layout + Deco",
        "Mixto",
        "Crea la jugabilidad y la decoracion",
        "al mismo tiempo.",
        "Construye primero el layout jugable y aplica encima una capa completa de decoracion, bordes, relleno e iluminacion en una sola orden.",
        {"Layout jugable completo", "Decoracion y detalles", "Triggers y canales de color"},
        {"Objetos sin sincronizar", "Colisiones ocultas"},
        "Crea una seccion de cubo y nave decorada al estilo futurista neon",
        "Actua en dos fases combinadas para Geometry Dash: primero construye un layout jugable solido y enseguida envuelvelo con decoracion estructurada, bordes definidos y triggers de iluminacion.",
        "mode-layout-deco.png"_spr,
        -7.f,
        ccc3(0xb4, 0x58, 0xf6),
        ccc3(0x1b, 0x10, 0x29),
        ModeIconType::LayoutDeco
    },
    {
        "block_design",
        "BLOCK DESIGN",
        "Block Design",
        "Bloques",
        "Disena bloques y estructuras",
        "personalizadas.",
        "Rellena tus estructuras con patrones internos, conectores, biseles, sombras y acentos luminosos adaptados al tamano de cada pilar.",
        {"Rellenos y patrones", "Bordes y biseles", "Brillo y acentos de color"},
        {"Portales y velocidades", "Trampas fuera del bloque"},
        "Disena un block design tecnologico con nucleo brillante para esta estructura",
        "Especialista en Block Design de Geometry Dash. Construye el interior y contorno de estructuras y pilares combinando patrones geometricos, bordes limpios, sombras internas y acentos de luz.",
        "mode-block-design.png"_spr,
        0.f,
        ccc3(0x9b, 0x5c, 0xf6),
        ccc3(0x18, 0x10, 0x28),
        ModeIconType::BlockDesign
    },
    {
        "triggers",
        "TRIGGERS",
        "Triggers",
        "Logica",
        "Configura y usa triggers de forma",
        "automatica.",
        "Calcula y coloca triggers Move, Pulse, Color, Alpha, Rotate, Spawn y Toggle asignando automaticamente los siguientes grupos y canales libres.",
        {"Move, Pulse, Color y Alpha", "Rotate, Scale y Toggle", "Grupos libres automaticos"},
        {"Bloques solidos", "Picos y obstaculos"},
        "Haz que la estructura seleccionada baje suavemente con easing y parpadee",
        "Ingeniero de Triggers de Geometry Dash 2.2. Configura secuencias precisas de triggers Move, Pulse, Color, Alpha, Rotate, Scale, Spawn y Toggle con tiempos exactos y curvas de easing naturales.",
        "mode-triggers.png"_spr,
        +7.f,
        ccc3(0xc0, 0x56, 0xf4),
        ccc3(0x1c, 0x0f, 0x28),
        ModeIconType::Triggers
    },
    {
        "transiciones",
        "TRANSICIONES",
        "Transiciones",
        "Visual",
        "Crea transiciones suaves y",
        "creativas.",
        "Conecta dos zonas del nivel o cambios de portal con cortinas de luz, destellos sincronizados, cambios de paleta y entradas fluidas.",
        {"Cortinas y barridos de luz", "Triggers de cambio de color", "Detalles de entrada y salida"},
        {"Muros injustos", "Cambios ciegos con picos"},
        "Haz una transicion suave de color y luz entre estas dos zonas",
        "Especialista en Transiciones de Geometry Dash. Conecta dos partes del nivel o cambios de modo con efectos de cortina, destellos sincronizados, camara suave y tiempo de reaccion justo.",
        "mode-transiciones.png"_spr,
        +14.f,
        ccc3(0xd2, 0x54, 0xee),
        ccc3(0x1d, 0x0f, 0x27),
        ModeIconType::Transiciones
    },
    {
        "efectos",
        "EFECTOS AVANZADOS",
        "Efectos",
        "Visual",
        "Anade efectos especiales,",
        "movimiento y mas.",
        "Crea fondos con parallax multicapa, ondas de pulso ritmico, particulas orbitales y secuencias visuales avanzadas.",
        {"Parallax y profundidad", "Ondas de pulso y brillo", "Triggers encadenados"},
        {"Colisiones fisicas", "Obstaculos letales"},
        "Anade un efecto de pulso neon y movimiento flotante al fondo",
        "Creador de Efectos Visuales Avanzados en Geometry Dash. Disena parallax multicapa, ondas de pulso sincronizadas, distorsiones rítmicas y fondos animados optimizados en objetos.",
        "mode-efectos.png"_spr,
        +21.f,
        ccc3(0xe0, 0x54, 0xe4),
        ccc3(0x1e, 0x0f, 0x25),
        ModeIconType::Efectos
    },
    {
        "ideas",
        "IDEAS Y SUGERENCIAS",
        "Ideas",
        "Consejos",
        "Obten ideas, mejoras y consejos",
        "para tu nivel.",
        "Analiza la composicion actual de tu nivel y te devuelve sugerencias concretas de paleta, ritmo, decoracion y estructura sin modificar tus objetos.",
        {"Analisis del nivel actual", "Sugerencias de paleta y estilo", "Ideas de estructura y ritmo"},
        {"No modifica tus objetos", "No altera grupos ni colores"},
        "Dame 3 ideas para mejorar la decoracion y el ambiente de esta parte",
        "Director Creativo y Asesor de Niveles para Geometry Dash. Analiza la idea del usuario y responde con sugerencias creativas concretas, paletas de colores y consejos de estructuracion (sin colocar objetos).",
        "mode-ideas.png"_spr,
        +28.f,
        ccc3(0xec, 0x56, 0xd4),
        ccc3(0x1f, 0x0f, 0x23),
        ModeIconType::Ideas
    }
};

inline int modeIndexById(std::string const& id) {
    for (int i = 0; i < 9; ++i) {
        if (id == MODES[i].id) return i;
    }
    return -1;
}

inline std::string trimStr(std::string s) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

inline std::string oneLineStr(std::string s) {
    for (char& c : s) {
        if (c == '\r' || c == '\n' || c == '\t') c = ' ';
    }
    return trimStr(s);
}

// Normaliza texto conservando un marcador ASCII para ñ ('~') y Ñ ('^')
inline std::string normalizePromptInputWithEnye(std::string const& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0xC3 && i + 1 < s.size()) {
            unsigned char n = static_cast<unsigned char>(s[i + 1]);
            ++i;
            switch (n) {
                case 0xA1: case 0xA0: case 0xA4: case 0xA2: out.push_back('a'); break;
                case 0xA9: case 0xA8: case 0xAB: case 0xAA: out.push_back('e'); break;
                case 0xAD: case 0xAC: case 0xAF: case 0xAE: out.push_back('i'); break;
                case 0xB3: case 0xB2: case 0xB6: case 0xB4: out.push_back('o'); break;
                case 0xBA: case 0xB9: case 0xBC: case 0xBB: out.push_back('u'); break;
                case 0xB1: out.push_back('~'); break; // ñ
                case 0x81: case 0x80: case 0x84: case 0x82: out.push_back('A'); break;
                case 0x89: case 0x88: case 0x8B: case 0x8A: out.push_back('E'); break;
                case 0x8D: case 0x8C: case 0x8F: case 0x8E: out.push_back('I'); break;
                case 0x93: case 0x92: case 0x96: case 0x94: out.push_back('O'); break;
                case 0x9A: case 0x99: case 0x9C: case 0x9B: out.push_back('U'); break;
                case 0x91: out.push_back('^'); break; // Ñ
                default: break;
            }
        } else if (c == 0xC2 && i + 1 < s.size()) {
            unsigned char n = static_cast<unsigned char>(s[i + 1]);
            ++i;
            if (n == 0xA1) out.push_back('!');
            else if (n == 0xBF) out.push_back('?');
            else if (n == 0xB7) out.push_back('.');
        } else if (c == 0xE2 && i + 2 < s.size()) {
            unsigned char n1 = static_cast<unsigned char>(s[i + 1]);
            unsigned char n2 = static_cast<unsigned char>(s[i + 2]);
            i += 2;
            if (n1 == 0x80 && (n2 == 0x93 || n2 == 0x94)) out.push_back('-');
            else if (n1 == 0x80 && (n2 == 0x9C || n2 == 0x9D)) out.push_back('"');
            else if (n1 == 0x80 && (n2 == 0x98 || n2 == 0x99)) out.push_back('\'');
            else if (n1 == 0x80 && n2 == 0xA2) out.push_back('*');
            else if (n1 == 0x80 && n2 == 0xA6) { out += "..."; }
        } else if (c >= 0xA0) {
            switch (c) {
                case 0xF1: out.push_back('~'); break;
                case 0xD1: out.push_back('^'); break;
                case 0xE1: case 0xE0: case 0xE4: case 0xE2: out.push_back('a'); break;
                case 0xE9: case 0xE8: case 0xEB: case 0xEA: out.push_back('e'); break;
                case 0xED: case 0xEC: case 0xEF: case 0xEE: out.push_back('i'); break;
                case 0xF3: case 0xF2: case 0xF6: case 0xF4: out.push_back('o'); break;
                case 0xFA: case 0xF9: case 0xFC: case 0xFB: out.push_back('u'); break;
                case 0xC1: case 0xC0: case 0xC4: case 0xC2: out.push_back('A'); break;
                case 0xC9: case 0xC8: case 0xCB: case 0xCA: out.push_back('E'); break;
                case 0xCD: case 0xCC: case 0xCF: case 0xCE: out.push_back('I'); break;
                case 0xD3: case 0xD2: case 0xD6: case 0xD4: out.push_back('O'); break;
                case 0xDA: case 0xD9: case 0xDC: case 0xDB: out.push_back('U'); break;
                case 0xA1: out.push_back('!'); break;
                case 0xBF: out.push_back('?'); break;
                default: break;
            }
        } else if (c >= 32 && c < 127) {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

inline std::string sanitizeForGD(std::string const& s) {
    std::string out = normalizePromptInputWithEnye(s);
    for (size_t i = 0; i < out.size(); ++i) {
        if (out[i] == '~') out[i] = 'n';
        else if (out[i] == '^') out[i] = 'N';
    }
    return out;
}

} // namespace decomind
