#pragma once
// ===========================================================================
// DecoMind AI - Capa de Gestion de Modos y Estado
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
        "Ordena grupos, capas del editor (Z-Layer / Z-Order) y canales de color sin alterar la jugabilidad.",
        "Agrupa las estructuras del primer bloque y ordena las capas de fondo y frente",
        "MODO ESTRUCTURAR. Analiza el nivel indicado (grupos, capas, canales de color, triggers, secciones) "
        "y organizalo: agrupa objetos relacionados, ordena capas, elimina triggers redundantes y libera IDs "
        "de grupo sin usar. No cambies la jugabilidad (colisiones, hitboxes, tiempos). Responde con un plan "
        "breve y despues con las operaciones a realizar.",
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
        "Crea un layout de cubo y nave de 20 segundos con ritmo medio",
        "MODO LAYOUT. Crea o rehaz SOLO la jugabilidad (bloques, picos, portales, pads, aros) con un ritmo "
        "coherente, para el modo de juego pedido. Respeta distancias y alturas superables, evita secciones "
        "imposibles y no anadas decoracion. Verifica que cada salto sea posible.",
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
        "Decora tus estructuras existentes con detalles visuales, atmosfera y contraste respetando las colisiones.",
        "Mejora la deco de esta parte con estilo neon azul y detalles brillantes",
        "MODO DECO. Decora SIN modificar la jugabilidad: no muevas, borres ni cambies bloques solidos, picos "
        "ni triggers que afecten colisiones. Usa solo objetos decorativos, colores y capas (z-order) "
        "coherentes con el estilo pedido. Respeta los grupos y canales de color que ya estan en uso.",
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
        "Genera una seccion completa combinando estructura jugable verificada y decoracion tematica.",
        "Crea una seccion de 15 segundos con layout dinamico y decoracion espacial",
        "MODO LAYOUT + DECO. Trabaja en dos pasadas: 1) crea la jugabilidad como en LAYOUT y comprueba que "
        "sea superable; 2) decora encima como en DECO sin alterar la jugabilidad ya validada. Informa de "
        "cada pasada por separado.",
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
        "Crea patrones internos, bordes, esquinas y rellenos modulares para tus estructuras y pilares.",
        "Disena un bloque tecnologico con bordes cian e interior geometrico oscuro",
        "MODO BLOCK DESIGN. Disena bloques y estructuras personalizadas (bordes, relleno, esquinas, detalles) "
        "con un estilo consistente. La forma jugable (hitbox) debe quedar identica a la original. Reutiliza "
        "piezas y agrupalas para poder duplicarlas.",
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
        "Automatiza triggers de Move, Pulse, Color, Alpha, Toggle y Spawn usando grupos libres de forma segura.",
        "Configura triggers de pulso y movimiento suave para las estructuras del grupo libre",
        "MODO TRIGGERS. Configura triggers (movimiento, color, pulso, alfa, toggle, spawn, etc.) para lograr "
        "el efecto pedido. Antes de crear grupos o canales de color, consulta cuales estan en uso y elige "
        "libres. Explica el orden y el tiempo de cada trigger.",
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
        "Conecta dos zonas o cambios de portal con efectos visuales fluidos sin entorpecer la lectura del jugador.",
        "Haz una transicion entre 2 zonas pasando de tono oscuro a cian brillante",
        "MODO TRANSICIONES. Crea transiciones suaves entre dos zonas o estilos del nivel (colores, fondo, "
        "escenario, modo de juego) usando triggers y decoracion, sin alterar la jugabilidad ni el tiempo de "
        "las secciones.",
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
        "Incorpora parallax, pulsos sincronizados, particulas y animaciones cuidando el rendimiento de objetos.",
        "Anade un fondo con efecto parallax suave y destellos sincronizados",
        "MODO EFECTOS AVANZADOS. Anade efectos especiales y movimiento (particulas, pulsos al ritmo, shake, "
        "parallax, animaciones de mover/rotar/escalar) de forma ordenada y sin saturar el nivel. No cambies "
        "la jugabilidad.",
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
        "Analiza tu nivel actual y ofrece recomendaciones de composicion, colores y ritmo sin modificar objetos.",
        "Dame ideas para mejorar el contraste y la atmosfera de esta seccion",
        "MODO IDEAS. No modifiques el nivel. Analiza lo que hay y propone ideas, mejoras y consejos concretos "
        "(ritmo, contraste, composicion, deco, triggers) ordenados de mayor a menor impacto.",
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
