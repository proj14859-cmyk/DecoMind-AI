#pragma once
// ===========================================================================
// DecoMind AI v0.3.4-alpha - Capa de Gestion de Modos y Estado
//
// Cada modo mantiene estrictamente separados tres elementos:
//   1. id             : Identificador interno ("estructurar", "layout", etc.)
//   2. visibleDesc    : Descripcion breve visible para el usuario en la UI
//   3. internalPrompt : Instrucciones internas (preprompt) para el modelo IA,
//                       que NUNCA se muestran en la interfaz visual.
// ===========================================================================
#include <Geode/Geode.hpp>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace decomind {

struct ModeInfo {
    const char* id;             // 1. Identificador interno
    const char* title;          // Titulo visible en mayusculas (tarjeta)
    const char* name;           // Nombre legible corto (panel lateral / estado)
    const char* tag;            // Abreviatura de respaldo
    const char* spriteName;     // Nombre del recurso sprite (_spr)
    const char* visibleLine1;   // 2a. Linea 1 de descripcion visible en tarjeta
    const char* visibleLine2;   // 2b. Linea 2 de descripcion visible en tarjeta
    const char* userGuide;      // Explicacion ampliada para el usuario en pestana MODOS (no es preprompt)
    const char* examplePrompt;  // Ejemplo de peticion sugerida para este modo
    const char* internalPrompt; // 3. Preprompt interno para el modelo de IA (jamas visible en UI)
    ccColor3B bgColor;          // Color de fondo de la tarjeta (fiel a la referencia)
    ccColor3B borderColor;      // Color de borde e icono neon (fiel a la referencia)
};

// Los 9 modos oficiales de DecoMind AI con colores exactos de la referencia
static const ModeInfo MODES[9] = {
    // 0: ESTRUCTURAR
    {
        "estructurar",
        "ESTRUCTURAR",
        "Estructurar",
        "ES",
        "mode-estructurar.png"_spr,
        "Organiza y estructura",
        "tu nivel de forma inteligente.",
        "Lee las estructuras seleccionadas o visibles y organiza grupos libres, canales de color y capas (Z-Layer / Z-Order) sin alterar la jugabilidad.",
        "Estructura esta seleccion asignando grupos libres para bordes, relleno y fondo con colores 10 a 14",
        "MODO ESTRUCTURAR. Objetivo: leer la seleccion o vista activa del nivel (SELECTION / VIEWPORT) y estructurar u organizar sus piezas con Group IDs libres (FREE_GROUPS), Color IDs libres (FREE_COLORS) y capas Z (Z-Layer / Z-Order) sin alterar las colisiones. Si el usuario pide reforzar o completar la estructura seleccionada con bordes, esquinas o triggers de organizacion, usa las coordenadas exactas de SELECTION.",
        ccc3(0x01, 0x2e, 0x3c),
        ccc3(0x01, 0xd3, 0xae)
    },
    // 1: LAYOUT
    {
        "layout",
        "LAYOUT",
        "Layout",
        "LY",
        "mode-layout.png"_spr,
        "Crea la estructura y",
        "la jugabilidad del nivel.",
        "Construye secciones jugables con saltos, portales, pads y orbes equilibrados y sincronizados.",
        "Crea un layout de cubo y nave de 20 segundos con ritmo medio continuando desde la camara",
        "MODO LAYOUT. Objetivo: crear SOLO la jugabilidad (bloques, picos, portales, pads, orbes) con ritmo coherente para el modo de juego pedido. Si hay objetos en SELECTION o VIEWPORT, continua desde su posicion o respeta su altura. Todo salto debe ser superable y deja siempre espacio libre para pasar. Sin decoracion.",
        ccc3(0x03, 0x1d, 0x64),
        ccc3(0x02, 0xa5, 0xfc)
    },
    // 2: DECO
    {
        "deco",
        "DECO",
        "Deco",
        "DC",
        "mode-deco.png"_spr,
        "Anade y mejora la",
        "decoracion del nivel.",
        "Lee las estructuras seleccionadas o el nivel visible y genera decoracion a medida respetando grupos y colores libres o indicados.",
        "Genera una deco estilo neon para esta estructura seleccionada usando colores del 10 al 14, grupo de objetos 20 al 25 y animacion de entrada y salida",
        "MODO DECO. Objetivo: decorar con estilo y atmosfera la estructura seleccionada (SELECTION) o la zona visible (VIEWPORT) SIN alterar la jugabilidad. Si hay objetos seleccionados, coloca bordes, esquinas, relleno interior, glow y detalles alrededor de su caja delimitadora (bbox). Si el usuario pide rangos de Group ID o Color ID, usa exactamente esos; si no, asigna IDs de FREE_GROUPS y FREE_COLORS sin pisar USED_GROUPS ni USED_COLORS. Si pide animacion de entrada o salida, anade triggers (Move 901, Alpha 1007, Pulse 1006, Color 899, Rotate 1346) apuntando al grupo de la decoracion.",
        ccc3(0x28, 0x0c, 0x66),
        ccc3(0xb6, 0x48, 0xf6)
    },
    // 3: LAYOUT + DECO
    {
        "layout_deco",
        "LAYOUT + DECO",
        "Layout + Deco",
        "L+D",
        "mode-layout-deco.png"_spr,
        "Crea la jugabilidad y",
        "decoracion al mismo tiempo.",
        "Genera una seccion completa combinando estructura jugable verificada, decoracion tematica y grupos/colores libres.",
        "Crea una seccion de 15 segundos con layout dinamico, decoracion espacial y animacion de entrada",
        "MODO LAYOUT + DECO. Objetivo: una seccion completa en dos pasadas: 1) jugabilidad superable como en LAYOUT; 2) decoracion como en DECO sobre las estructuras creadas usando FREE_GROUPS y FREE_COLORS (o los rangos pedidos por el usuario) sin tapar picos ni la ruta de salto. Informa de cada pasada por separado.",
        ccc3(0x30, 0x20, 0x0e),
        ccc3(0xfb, 0xcf, 0x50)
    },
    // 4: BLOCK DESIGN
    {
        "block_design",
        "BLOCK DESIGN",
        "Block Design",
        "BL",
        "mode-block-design.png"_spr,
        "Disena bloques y",
        "estructuras personalizadas.",
        "Disena patrones internos, bordes, esquinas y rellenos modulares sobre tu estructura seleccionada o en una pieza nueva.",
        "Disena un block design tecnologico para esta estructura seleccionada usando colores 12 a 15 y grupo 30",
        "MODO BLOCK DESIGN. Objetivo: disenar bloques y estructuras personalizadas (bordes, esquinas, relleno geometrico, glow y detalles). Si el usuario tiene una estructura seleccionada (SELECTION), aplica el Block Design exactamente sobre las coordenadas y bordes de esa estructura; si no hay seleccion, crea una estructura modular en la vista actual. Respeta los Group IDs y Color IDs pedidos o usa FREE_GROUPS y FREE_COLORS.",
        ccc3(0x3f, 0x11, 0x32),
        ccc3(0xf7, 0x54, 0x75)
    },
    // 5: TRIGGERS
    {
        "triggers",
        "TRIGGERS",
        "Triggers",
        "TG",
        "mode-triggers.png"_spr,
        "Configura y usa triggers",
        "de forma automatica.",
        "Automatiza triggers de Move, Pulse, Color, Alpha, Rotate, Scale, Toggle y Spawn leyendo los grupos y colores libres del nivel.",
        "Anade una animacion de entrada con Move y Alpha y un pulso de color para el grupo 20 usando canales libres",
        "MODO TRIGGERS. Objetivo: configurar triggers oficiales de GD 2.2 (Color 899, Move 901, Pulse 1006, Alpha 1007, Toggle 1049, Spawn 1268, Rotate 1346, Follow 1347, Stop 1616, Scale 2067) para animar las estructuras seleccionadas o los grupos indicados. Revisa USED_GROUPS, FREE_GROUPS, USED_COLORS y FREE_COLORS para no colisionar con grupos ni colores ocupados, y explica que IDs y tiempos usaste.",
        ccc3(0x01, 0x34, 0x57),
        ccc3(0x00, 0xe3, 0xf7)
    },
    // 6: TRANSICIONES
    {
        "transiciones",
        "TRANSICIONES",
        "Transiciones",
        "TR",
        "mode-transiciones.png"_spr,
        "Crea transiciones suaves",
        "y creativas.",
        "Conecta dos zonas o cambios de portal con efectos visuales fluidos y triggers sincronizados en grupos y colores libres.",
        "Haz una transicion entre 2 zonas pasando de tono oscuro a cian brillante con entrada y salida suave",
        "MODO TRANSICIONES. Objetivo: transicion suave entre dos zonas o estilos (color, fondo, cortina de luz, alpha, move) con triggers y objetos decorativos sin alterar la jugabilidad. Usa los grupos y canales de color pedidos por el usuario o toma IDs de FREE_GROUPS y FREE_COLORS.",
        ccc3(0x19, 0x0e, 0x5b),
        ccc3(0xa9, 0x4e, 0xf8)
    },
    // 7: EFECTOS AVANZADOS
    {
        "efectos",
        "EFECTOS AVANZADOS",
        "Efectos Avanzados",
        "FX",
        "mode-efectos.png"_spr,
        "Anade efectos especiales,",
        "movimiento y mas.",
        "Incorpora animaciones de entrada/salida, parallax, pulsos sincronizados y rotacion sobre tu seleccion o vista actual.",
        "Anade un efecto de entrada y salida suave con destellos y rotacion a la estructura seleccionada",
        "MODO EFECTOS AVANZADOS. Objetivo: efectos visuales (animaciones de entrada/salida con Move/Alpha/Scale, pulsos al ritmo, rotacion, parallax) sobre la estructura seleccionada (SELECTION) o la vista activa (VIEWPORT), usando Group IDs y Color IDs libres o los rangos indicados por el usuario.",
        ccc3(0x3b, 0x1b, 0x17),
        ccc3(0xfa, 0x92, 0x37)
    },
    // 8: IDEAS Y SUGERENCIAS
    {
        "ideas",
        "IDEAS Y SUGERENCIAS",
        "Ideas y Sugerencias",
        "ID",
        "mode-ideas.png"_spr,
        "Obten ideas, mejoras",
        "y consejos para tu nivel.",
        "Analiza los objetos seleccionados, grupos ocupados y colores de tu nivel actual y ofrece recomendaciones concretas.",
        "Analiza lo que tengo seleccionado y dime como mejorar su deco, colores y animaciones",
        "MODO IDEAS. No modificas el nivel. Analiza el contexto real enviado del nivel (SELECTION, VIEWPORT, USED_GROUPS, FREE_GROUPS, USED_COLORS, FREE_COLORS) y da de 3 a 5 ideas concretas y accionables (deco, block design, paleta de colores y triggers con IDs libres especificos) ordenadas de mayor a menor impacto.",
        ccc3(0x01, 0x14, 0x51),
        ccc3(0x00, 0x9b, 0xf7)
    }
};

inline int modeIndexById(std::string const& id) {
    for (int i = 0; i < 9; ++i) {
        if (id == MODES[i].id) return i;
    }
    return -1;
}

// Convierte cadenas UTF-8 con tildes o caracteres especiales a ASCII limpio
// compatible con las fuentes bitmap de Geometry Dash (bigFont.fnt / chatFont.fnt),
// evitando que se omitan letras acentuadas.
inline std::string sanitizeForGD(std::string const& input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (c == 0xC3 && i + 1 < input.size()) {
            unsigned char n = static_cast<unsigned char>(input[i + 1]);
            switch (n) {
                case 0xA1: case 0xA0: case 0xA2: case 0xA4: out += 'a'; ++i; continue;
                case 0xA9: case 0xA8: case 0xAA: case 0xAB: out += 'e'; ++i; continue;
                case 0xAD: case 0xAC: case 0xAE: case 0xAF: out += 'i'; ++i; continue;
                case 0xB3: case 0xB2: case 0xB4: case 0xB6: out += 'o'; ++i; continue;
                case 0xBA: case 0xB9: case 0xBB: case 0xBC: out += 'u'; ++i; continue;
                case 0xB1: out += 'n'; ++i; continue;
                case 0x81: case 0x80: case 0x82: case 0x84: out += 'A'; ++i; continue;
                case 0x89: case 0x88: case 0x8A: case 0x8B: out += 'E'; ++i; continue;
                case 0x8D: case 0x8C: case 0x8E: case 0x8F: out += 'I'; ++i; continue;
                case 0x93: case 0x92: case 0x94: case 0x96: out += 'O'; ++i; continue;
                case 0x9A: case 0x99: case 0x9B: case 0x9C: out += 'U'; ++i; continue;
                case 0x91: out += 'N'; ++i; continue;
                default: break;
            }
        } else if (c == 0xC2 && i + 1 < input.size()) {
            unsigned char n = static_cast<unsigned char>(input[i + 1]);
            if (n == 0xA1) { out += '!'; ++i; continue; }
            if (n == 0xBF) { out += '?'; ++i; continue; }
        }
        if (c >= 32 && c < 127) {
            out += static_cast<char>(c);
        } else if (c == '\n' || c == '\t') {
            out += static_cast<char>(c);
        }
    }
    return out;
}

inline std::string trimStr(std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

inline std::string oneLineStr(std::string s) {
    for (auto& c : s) {
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    }
    return s;
}

} // namespace decomind
