#pragma once
// ===========================================================================
// DecoMind AI v0.3.7-alpha - Motor de Renderizado Visual, Paleta Cromatica
//                            ("Un color -> toda la paleta") y Gestor de Fuentes
//
// Implementa fielmente las 6 paginas de DecoMind_AI_interfaz_pulida.pdf:
//   - Receta cromatica HSL completa a partir del matiz H elegido (Pagina 6):
//       Fondo exterior (50/5), Panel (42/8), Tarjeta (36/12), Elevado (32/17),
//       Linea (30/22), Linea fuerte (35/32), Acento (92/64), Acento suave (95/78),
//       Acento oscuro (75/42), Tinta sobre acento (70/8), Texto (25/97),
//       Texto 2 (20/76) y Texto 3 (16/62).
//   - Tonos vecinos (±28 grados de matiz) para distinguir los 9 modos sin salir
//     de la escala cromatica activa.
//   - Sistema de Fuentes Personalizadas del Juego (chatFont, bigFont, goldFont y
//     las 59 fuentes gjFont01.fnt .. gjFont59.fnt de lib.zip) con conversion
//     automatica a mayusculas en las fuentes que carecen de glifos minusculos.
// ===========================================================================
#include "Modes.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>

namespace decomind {

static constexpr float PW = 480.f;
static constexpr float PH = 320.f;
static constexpr float REF_W = 1536.f;
static constexpr float REF_H = 1024.f;
static constexpr float SCALE_K = PW / REF_W; // 0.3125f exacto en X e Y

inline constexpr float px(float x_ref) {
    return x_ref * SCALE_K;
}
inline constexpr float py(float y_top_ref) {
    return (REF_H - y_top_ref) * SCALE_K;
}
inline constexpr float ps(float len_ref) {
    return len_ref * SCALE_K;
}

inline ccColor4B rgba4B(ccColor3B c, GLubyte a = 255) {
    return ccc4(c.r, c.g, c.b, a);
}

inline ccColor4F rgba4F(ccColor3B c, float a = 1.f) {
    return ccc4f(c.r / 255.f, c.g / 255.f, c.b / 255.f, a);
}

inline ccColor3B lerpColor(ccColor3B a, ccColor3B b, float t) {
    t = std::max(0.f, std::min(1.f, t));
    return ccc3(
        static_cast<GLubyte>(a.r + (b.r - a.r) * t),
        static_cast<GLubyte>(a.g + (b.g - a.g) * t),
        static_cast<GLubyte>(a.b + (b.b - a.b) * t)
    );
}

inline void rgbToHsv(ccColor3B c, float& h, float& s, float& v) {
    float r = c.r / 255.f, g = c.g / 255.f, b = c.b / 255.f;
    float mx = std::max({r, g, b}), mn = std::min({r, g, b});
    float d = mx - mn;
    v = mx;
    s = (mx <= 0.0001f) ? 0.f : (d / mx);
    if (d <= 0.0001f) {
        h = 0.f;
    } else if (mx == r) {
        h = (g - b) / d + (g < b ? 6.f : 0.f);
        h /= 6.f;
    } else if (mx == g) {
        h = ((b - r) / d + 2.f) / 6.f;
    } else {
        h = ((r - g) / d + 4.f) / 6.f;
    }
}

inline ccColor3B hsvToRgbColor(float h, float s, float v) {
    h = h - std::floor(h);
    s = std::max(0.f, std::min(1.f, s));
    v = std::max(0.f, std::min(1.f, v));
    int i = static_cast<int>(h * 6.f);
    float f = h * 6.f - static_cast<float>(i);
    float p = v * (1.f - s);
    float q = v * (1.f - f * s);
    float t = v * (1.f - (1.f - f) * s);
    float r = v, g = t, b = p;
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return ccc3(
        static_cast<GLubyte>(r * 255.f),
        static_cast<GLubyte>(g * 255.f),
        static_cast<GLubyte>(b * 255.f)
    );
}

// Conversion exacta HSL (grados 0..360, saturacion 0..100, luz 0..100) -> RGB
// Reproduce al 100% la tabla "Un color -> toda la paleta" de la Pagina 6 del PDF
inline ccColor3B hslToRgb(float hDeg, float sPct, float lPct) {
    float h = std::fmod(hDeg, 360.f);
    if (h < 0.f) h += 360.f;
    float s = std::clamp(sPct / 100.f, 0.f, 1.f);
    float l = std::clamp(lPct / 100.f, 0.f, 1.f);

    float c = (1.f - std::fabs(2.f * l - 1.f)) * s;
    float hPrime = h / 60.f;
    float x = c * (1.f - std::fabs(std::fmod(hPrime, 2.f) - 1.f));
    float r1 = 0.f, g1 = 0.f, b1 = 0.f;

    if (hPrime < 1.f)      { r1 = c; g1 = x; b1 = 0.f; }
    else if (hPrime < 2.f) { r1 = x; g1 = c; b1 = 0.f; }
    else if (hPrime < 3.f) { r1 = 0.f; g1 = c; b1 = x; }
    else if (hPrime < 4.f) { r1 = 0.f; g1 = x; b1 = c; }
    else if (hPrime < 5.f) { r1 = x; g1 = 0.f; b1 = c; }
    else                   { r1 = c; g1 = 0.f; b1 = x; }

    float m = l - c * 0.5f;
    auto toByte = [](float v) -> GLubyte {
        return static_cast<GLubyte>(std::clamp(std::lround(v * 255.f), 0L, 255L));
    };
    return ccc3(toByte(r1 + m), toByte(g1 + m), toByte(b1 + m));
}

inline float extractHueDeg(ccColor3B c, float fallbackHue = 265.f) {
    float h = 0.f, s = 0.f, v = 0.f;
    rgbToHsv(c, h, s, v);
    if (s < 0.04f) return fallbackHue;
    return h * 360.f;
}

// ===========================================================================
// Sistema Cromatico Completo ("Un color -> toda la paleta", Pagina 6 del PDF)
// ===========================================================================
struct ChromaticPalette {
    float hueDeg = 265.f;
    ccColor3B bgOuter;      // Fondo exterior: marco del menu (50 / 5)
    ccColor3B panel;        // Panel: barras y columnas (42 / 8)
    ccColor3B card;         // Tarjeta: modos, bloques (36 / 12)
    ccColor3B elevated;     // Elevado: seleccion, hover (32 / 17)
    ccColor3B line;         // Linea: bordes finos (30 / 22)
    ccColor3B lineStrong;   // Linea fuerte: campos, enfasis (35 / 32)
    ccColor3B accent;       // Acento: botones, activo (92 / 64)
    ccColor3B accentSoft;   // Acento suave: iconos, enlaces (95 / 78)
    ccColor3B accentDark;   // Acento oscuro: sombras, pinchos (75 / 42)
    ccColor3B inkOnAccent;  // Tinta sobre acento: texto en botones (70 / 8)
    ccColor3B text;         // Texto: titulos (25 / 97)
    ccColor3B text2;        // Texto 2: descripciones (20 / 76)
    ccColor3B text3;        // Texto 3: ayudas, hora (16 / 62)

    static ChromaticPalette fromHue(float hDeg) {
        ChromaticPalette p;
        p.hueDeg      = std::fmod(hDeg + 360.f, 360.f);
        p.bgOuter     = hslToRgb(p.hueDeg, 50.f,  5.f);
        p.panel       = hslToRgb(p.hueDeg, 42.f,  8.f);
        p.card        = hslToRgb(p.hueDeg, 36.f, 12.f);
        p.elevated    = hslToRgb(p.hueDeg, 32.f, 17.f);
        p.line        = hslToRgb(p.hueDeg, 30.f, 22.f);
        p.lineStrong  = hslToRgb(p.hueDeg, 35.f, 32.f);
        p.accent      = hslToRgb(p.hueDeg, 92.f, 64.f);
        p.accentSoft  = hslToRgb(p.hueDeg, 95.f, 78.f);
        p.accentDark  = hslToRgb(p.hueDeg, 75.f, 42.f);
        p.inkOnAccent = hslToRgb(p.hueDeg, 70.f,  8.f);
        p.text        = hslToRgb(p.hueDeg, 25.f, 97.f);
        p.text2       = hslToRgb(p.hueDeg, 20.f, 76.f);
        p.text3       = hslToRgb(p.hueDeg, 16.f, 62.f);
        return p;
    }

    ccColor3B modeAccent(int modeIdx) const {
        float off = (modeIdx >= 0 && modeIdx < 9) ? MODES[modeIdx].hueOffsetDeg : 0.f;
        return hslToRgb(hueDeg + off, 92.f, 64.f);
    }

    ccColor3B modeAccentSoft(int modeIdx) const {
        float off = (modeIdx >= 0 && modeIdx < 9) ? MODES[modeIdx].hueOffsetDeg : 0.f;
        return hslToRgb(hueDeg + off, 95.f, 78.f);
    }

    ccColor3B modeBadgeFill(int modeIdx) const {
        float off = (modeIdx >= 0 && modeIdx < 9) ? MODES[modeIdx].hueOffsetDeg : 0.f;
        return hslToRgb(hueDeg + off, 38.f, 18.f);
    }
};

// Los 8 tonos predefinidos de la fila "Color del menu" en Ajustes (Pagina 5)
static constexpr float PRESET_HUES[8] = {
    265.f, // Violeta (#9B5CF6)
    356.f, // Rojo
     22.f, // Naranja
    148.f, // Esmeralda
    190.f, // Cian
    330.f, // Rosa
     44.f, // Ambar
    212.f  // Azul
};

// Colores de estado fijos (Regla 4 del PDF: Verde, ambar y rojo de estado no cambian con el tema)
static const ccColor3B COL_STATUS_GREEN = ccc3(0x34, 0xd3, 0x99);
static const ccColor3B COL_STATUS_AMBER = ccc3(0xfb, 0xbf, 0x24);
static const ccColor3B COL_STATUS_RED   = ccc3(0xf8, 0x71, 0x71);

// Constantes de compatibilidad usadas por popups secundarios
static const ccColor3B COL_BG_OUTER      = ccc3(0x09, 0x06, 0x0f);
static const ccColor3B COL_BG_FRAME      = ccc3(0x11, 0x0c, 0x1c);
static const ccColor3B COL_BG_CENTER     = ccc3(0x18, 0x12, 0x26);
static const ccColor3B COL_BG_RIGHT_CARD = ccc3(0x18, 0x12, 0x26);
static const ccColor3B COL_BG_INPUT      = ccc3(0x09, 0x06, 0x0f);
static const ccColor3B COL_BG_SIDE_IDLE  = ccc3(0x18, 0x12, 0x26);
static const ccColor3B COL_BG_SIDE_ACT   = ccc3(0x9b, 0x5c, 0xf6);
static const ccColor3B COL_BG_BOTTOM     = ccc3(0x11, 0x0c, 0x1c);
static const ccColor3B COL_BG_ALLY       = ccc3(0x11, 0x0c, 0x1c);
static const ccColor3B COL_FRAME_BLUE    = ccc3(0x9b, 0x5c, 0xf6);
static const ccColor3B COL_FRAME_CYAN    = ccc3(0xc4, 0x9b, 0xfc);
static const ccColor3B COL_NEON_GREEN    = ccc3(0x34, 0xd3, 0x99);
static const ccColor3B COL_BAR_TEAL      = ccc3(0x9b, 0x5c, 0xf6);
static const ccColor3B COL_TEXT_WHITE    = ccc3(0xf8, 0xf6, 0xfb);
static const ccColor3B COL_TEXT_CYAN     = ccc3(0xc4, 0x9b, 0xfc);
static const ccColor3B COL_TEXT_SOFT     = ccc3(0xc2, 0xb8, 0xcf);
static const ccColor3B COL_TEXT_DIM      = ccc3(0x9c, 0x92, 0xa8);
static const ccColor3B COL_DIVIDER       = ccc3(0x36, 0x28, 0x49);

// ===========================================================================
// Catalogo de Fuentes Disponibles del Juego (extraido de lib.zip / lib/fonts/)
// ===========================================================================
struct GameFontEntry {
    char const* displayName;
    char const* fntFile;
    bool uppercaseOnly;
    float scaleMul;
};

static const GameFontEntry GAME_FONT_LIST[] = {
    {"Por defecto (Chat)",  "chatFont.fnt",   false, 1.00f},
    {"Pusab (Oficial)",     "bigFont.fnt",    false, 0.62f},
    {"Dorada (Gold)",       "goldFont.fnt",   false, 0.62f},
    {"Fuente 01 (Pusab)",   "gjFont01.fnt",   true,  0.62f},
    {"Fuente 02 (Tragic)",  "gjFont02.fnt",   false, 0.66f},
    {"Fuente 03 (Geo)",     "gjFont03.fnt",   false, 0.66f},
    {"Fuente 04 (Bionic)",  "gjFont04.fnt",   false, 0.66f},
    {"Fuente 05 (Pixel)",   "gjFont05.fnt",   false, 0.68f},
    {"Fuente 06 (Stencil)", "gjFont06.fnt",   false, 0.66f},
    {"Fuente 07 (Digital)", "gjFont07.fnt",   false, 0.68f},
    {"Fuente 08 (Futura)",  "gjFont08.fnt",   false, 0.68f},
    {"Fuente 09 (Arcade)",  "gjFont09.fnt",   false, 0.68f},
    {"Fuente 10 (Sci-Fi)",  "gjFont10.fnt",   true,  0.66f},
    {"Fuente 11 (Neon)",    "gjFont11.fnt",   false, 0.68f},
    {"Fuente 12 (Gargle)",  "gjFont12.fnt",   false, 0.68f},
    {"Fuente 13 (Amatic)",  "gjFont13.fnt",   false, 0.72f},
    {"Fuente 14 (Cartwhl)", "gjFont14.fnt",   true,  0.66f},
    {"Fuente 15 (Script)",  "gjFont15.fnt",   false, 0.70f},
    {"Fuente 16 (Lemon)",   "gjFont16.fnt",   true,  0.64f},
    {"Fuente 17 (Lemon 2)", "gjFont17.fnt",   true,  0.64f},
    {"Fuente 18 (Minecrft)","gjFont18.fnt",   false, 0.68f},
    {"Fuente 19 (Optimus)", "gjFont19.fnt",   false, 0.68f},
    {"Fuente 20 (Autolova)","gjFont20.fnt",   false, 0.68f},
    {"Fuente 21 (Karate)",  "gjFont21.fnt",   false, 0.68f},
    {"Fuente 22 (Annyeong)","gjFont22.fnt",   false, 0.68f},
    {"Fuente 23 (Ausweis)", "gjFont23.fnt",   true,  0.64f},
    {"Fuente 24 (Gypsy)",   "gjFont24.fnt",   false, 0.68f},
    {"Fuente 25 (Magic)",   "gjFont25.fnt",   false, 0.70f},
    {"Fuente 26 (OldEng)",  "gjFont26.fnt",   false, 0.68f},
    {"Fuente 27 (YeahPapa)","gjFont27.fnt",   true,  0.66f},
    {"Fuente 28 (Ninja)",   "gjFont28.fnt",   true,  0.66f},
    {"Fuente 29 (Metal)",   "gjFont29.fnt",   true,  0.66f},
    {"Fuente 30 (DripDrop)","gjFont30.fnt",   false, 0.68f},
    {"Fuente 31 (Electro)", "gjFont31.fnt",   true,  0.64f},
    {"Fuente 32 (Aladin)",  "gjFont32.fnt",   false, 0.68f},
    {"Fuente 33 (Creepstr)","gjFont33.fnt",   true,  0.66f},
    {"Fuente 34 (CallOps)", "gjFont34.fnt",   true,  0.64f},
    {"Fuente 35 (BlocPrty)","gjFont35.fnt",   false, 0.68f},
    {"Fuente 36 (Astron)",  "gjFont36.fnt",   false, 0.68f},
    {"Fuente 37 (Osaka)",   "gjFont37.fnt",   true,  0.64f},
    {"Fuente 38 (SomeTime)","gjFont38.fnt",   false, 0.68f},
    {"Fuente 39 (Fatboy)",  "gjFont39.fnt",   false, 0.68f},
    {"Fuente 40 (Moria)",   "gjFont40.fnt",   true,  0.64f},
    {"Fuente 41 (Kingdom)", "gjFont41.fnt",   false, 0.68f},
    {"Fuente 42 (Fantaisi)","gjFont42.fnt",   false, 0.68f},
    {"Fuente 43 (Galaxy)",  "gjFont43.fnt",   false, 0.66f},
    {"Fuente 44 (WashHand)","gjFont44.fnt",   false, 0.68f},
    {"Fuente 45 (Bitwise)", "gjFont45.fnt",   false, 0.68f},
    {"Fuente 46 (FoulFnd)", "gjFont46.fnt",   true,  0.64f},
    {"Fuente 47 (Western)", "gjFont47.fnt",   true,  0.64f},
    {"Fuente 48 (EvilEmp)", "gjFont48.fnt",   true,  0.64f},
    {"Fuente 49 (Cartoon)", "gjFont49.fnt",   true,  0.64f},
    {"Fuente 50 (Carton6)", "gjFont50.fnt",   true,  0.64f},
    {"Fuente 51 (Assassin)","gjFont51.fnt",   false, 0.68f},
    {"Fuente 52 (PubPixel)","gjFont52.fnt",   false, 0.66f},
    {"Fuente 53 (DisneyUI)","gjFont53.fnt",   true,  0.64f},
    {"Fuente 54 (Random5)", "gjFont54.fnt",   false, 0.68f},
    {"Fuente 55 (Crafting)","gjFont55.fnt",   false, 0.68f},
    {"Fuente 56 (Squids)",  "gjFont56.fnt",   false, 0.66f},
    {"Fuente 57 (Monster)", "gjFont57.fnt",   false, 0.68f},
    {"Fuente 58 (Sumires)", "gjFont58.fnt",   false, 0.68f},
    {"Fuente 59 (Gewtymol)","gjFont59.fnt",   false, 0.68f}
};

static constexpr int GAME_FONT_COUNT = static_cast<int>(sizeof(GAME_FONT_LIST) / sizeof(GAME_FONT_LIST[0]));

// ===========================================================================
// Configuracion Persistente de Tema, Paleta, Boton Flotante y Fuente
// ===========================================================================
struct ThemeConfig {
    static ccColor3B getMenuAccent() {
        // Por defecto #9B5CF6 (Violeta H=265 del PDF: R=155, G=92, B=246)
        int r = Mod::get()->getSavedValue<int>("theme_r", 155);
        int g = Mod::get()->getSavedValue<int>("theme_g", 92);
        int b = Mod::get()->getSavedValue<int>("theme_b", 246);
        return ccc3(
            static_cast<GLubyte>(std::clamp(r, 0, 255)),
            static_cast<GLubyte>(std::clamp(g, 0, 255)),
            static_cast<GLubyte>(std::clamp(b, 0, 255))
        );
    }

    static void setMenuAccent(ccColor3B c) {
        Mod::get()->setSavedValue<int>("theme_r", c.r);
        Mod::get()->setSavedValue<int>("theme_g", c.g);
        Mod::get()->setSavedValue<int>("theme_b", c.b);
        (void)Mod::get()->saveData();
    }

    static ChromaticPalette getPalette() {
        ccColor3B acc = getMenuAccent();
        float h = extractHueDeg(acc, 265.f);
        return ChromaticPalette::fromHue(h);
    }

    static ccColor3B getRobotRgbColor() {
        auto pal = getPalette();
        int r = Mod::get()->getSavedValue<int>("robot_col_r", -1);
        int g = Mod::get()->getSavedValue<int>("robot_col_g", -1);
        int b = Mod::get()->getSavedValue<int>("robot_col_b", -1);
        if (r < 0 || g < 0 || b < 0) return pal.accent;
        return ccc3(
            static_cast<GLubyte>(std::clamp(r, 0, 255)),
            static_cast<GLubyte>(std::clamp(g, 0, 255)),
            static_cast<GLubyte>(std::clamp(b, 0, 255))
        );
    }

    static void setRobotRgbColor(ccColor3B c) {
        Mod::get()->setSavedValue<int>("robot_col_r", c.r);
        Mod::get()->setSavedValue<int>("robot_col_g", c.g);
        Mod::get()->setSavedValue<int>("robot_col_b", c.b);
        (void)Mod::get()->saveData();
    }

    static void resetRobotRgbToTheme() {
        Mod::get()->setSavedValue<int>("robot_col_r", -1);
        Mod::get()->setSavedValue<int>("robot_col_g", -1);
        Mod::get()->setSavedValue<int>("robot_col_b", -1);
        (void)Mod::get()->saveData();
    }

    static ccColor3B getSpikesRgbColor() {
        auto pal = getPalette();
        int r = Mod::get()->getSavedValue<int>("spikes_col_r", -1);
        int g = Mod::get()->getSavedValue<int>("spikes_col_g", -1);
        int b = Mod::get()->getSavedValue<int>("spikes_col_b", -1);
        if (r < 0 || g < 0 || b < 0) return pal.accent;
        return ccc3(
            static_cast<GLubyte>(std::clamp(r, 0, 255)),
            static_cast<GLubyte>(std::clamp(g, 0, 255)),
            static_cast<GLubyte>(std::clamp(b, 0, 255))
        );
    }

    static void setSpikesRgbColor(ccColor3B c) {
        Mod::get()->setSavedValue<int>("spikes_col_r", c.r);
        Mod::get()->setSavedValue<int>("spikes_col_g", c.g);
        Mod::get()->setSavedValue<int>("spikes_col_b", c.b);
        (void)Mod::get()->saveData();
    }

    static void resetSpikesRgbToTheme() {
        Mod::get()->setSavedValue<int>("spikes_col_r", -1);
        Mod::get()->setSavedValue<int>("spikes_col_g", -1);
        Mod::get()->setSavedValue<int>("spikes_col_b", -1);
        (void)Mod::get()->saveData();
    }

    static bool isRgbEnabled() {
        return Mod::get()->getSavedValue<bool>("rgb_enabled", true);
    }

    static void setRgbEnabled(bool en) {
        Mod::get()->setSavedValue<bool>("rgb_enabled", en);
        (void)Mod::get()->saveData();
    }

    static float getRgbSpeed() {
        int sp = Mod::get()->getSavedValue<int>("rgb_speed_pct", 100);
        return std::clamp(sp, 25, 300) / 100.f;
    }

    static int getRgbSpeedPct() {
        return std::clamp(Mod::get()->getSavedValue<int>("rgb_speed_pct", 100), 25, 300);
    }

    static void setRgbSpeedPct(int pct) {
        Mod::get()->setSavedValue<int>("rgb_speed_pct", std::clamp(pct, 25, 300));
        (void)Mod::get()->saveData();
    }

    static int getEditorBtnOpacityPct() {
        return std::clamp(Mod::get()->getSavedValue<int>("editor_btn_opacity_pct", 55), 20, 100);
    }

    static void setEditorBtnOpacityPct(int pct) {
        Mod::get()->setSavedValue<int>("editor_btn_opacity_pct", std::clamp(pct, 20, 100));
        (void)Mod::get()->saveData();
    }

    static int getFontIndex() {
        int idx = Mod::get()->getSavedValue<int>("ui_font_index", 0);
        return std::clamp(idx, 0, GAME_FONT_COUNT - 1);
    }

    static void setFontIndex(int idx) {
        Mod::get()->setSavedValue<int>("ui_font_index", std::clamp(idx, 0, GAME_FONT_COUNT - 1));
        (void)Mod::get()->saveData();
    }

    static GameFontEntry const& getActiveFontEntry() {
        return GAME_FONT_LIST[getFontIndex()];
    }

    static char const* getActiveFontFile(bool isBigHeader = false) {
        int idx = getFontIndex();
        if (idx == 0) {
            return isBigHeader ? "bigFont.fnt" : "chatFont.fnt";
        }
        return GAME_FONT_LIST[idx].fntFile;
    }

    static float getActiveFontScaleMul(bool isBigHeader = false) {
        int idx = getFontIndex();
        if (idx == 0) return 1.0f;
        return isBigHeader ? 1.0f : GAME_FONT_LIST[idx].scaleMul;
    }

    static std::string formatTextForActiveFont(std::string const& raw) {
        std::string clean = sanitizeForGD(raw);
        if (getActiveFontEntry().uppercaseOnly) {
            for (char& c : clean) {
                if (c >= 'a' && c <= 'z') {
                    c = static_cast<char>(c - 'a' + 'A');
                }
            }
        }
        return clean;
    }

    static ccColor3B getClipboardColor() {
        int r = Mod::get()->getSavedValue<int>("clip_col_r", 155);
        int g = Mod::get()->getSavedValue<int>("clip_col_g", 92);
        int b = Mod::get()->getSavedValue<int>("clip_col_b", 246);
        return ccc3(
            static_cast<GLubyte>(std::clamp(r, 0, 255)),
            static_cast<GLubyte>(std::clamp(g, 0, 255)),
            static_cast<GLubyte>(std::clamp(b, 0, 255))
        );
    }

    static void setClipboardColor(ccColor3B c) {
        Mod::get()->setSavedValue<int>("clip_col_r", c.r);
        Mod::get()->setSavedValue<int>("clip_col_g", c.g);
        Mod::get()->setSavedValue<int>("clip_col_b", c.b);
        (void)Mod::get()->saveData();
    }
};

namespace ui {

inline std::vector<CCPoint> makeRoundedRectVerts(
    float x, float y, float w, float h, float r, int segsPerCorner = 6
) {
    float maxR = std::min(w, h) * 0.48f;
    if (r > maxR) r = maxR;
    if (r < 0.5f) {
        return {ccp(x, y), ccp(x + w, y), ccp(x + w, y + h), ccp(x, y + h)};
    }

    std::vector<CCPoint> pts;
    pts.reserve(static_cast<size_t>((segsPerCorner + 1) * 4));
    const float pi = 3.14159265f;
    struct Corner { float cx; float cy; float startAngle; };
    Corner corners[4] = {
        {x + w - r, y + h - r, 0.f},
        {x + r,     y + h - r, pi * 0.5f},
        {x + r,     y + r,     pi},
        {x + w - r, y + r,     pi * 1.5f}
    };
    for (int c = 0; c < 4; ++c) {
        for (int s = 0; s <= segsPerCorner; ++s) {
            float ang = corners[c].startAngle + (pi * 0.5f) * (static_cast<float>(s) / static_cast<float>(segsPerCorner));
            pts.push_back(ccp(
                corners[c].cx + std::cos(ang) * r,
                corners[c].cy + std::sin(ang) * r
            ));
        }
    }
    return pts;
}

inline void drawRoundedRect(
    CCDrawNode* dn,
    float x, float y, float w, float h, float r,
    ccColor4F fill, float borderW, ccColor4F border
) {
    auto pts = makeRoundedRectVerts(x, y, w, h, r, 6);
    dn->drawPolygon(pts.data(), static_cast<unsigned int>(pts.size()), fill, borderW, border);
}

inline void drawWormPerimeterBorder(
    CCDrawNode* dn,
    float x, float y, float w, float h, float r,
    ccColor3B baseBorderCol,
    float travelPhase,
    bool wormEnabled,
    bool rainbowHue,
    float lineThickness
) {
    if (!dn) return;
    dn->clear();

    auto cornerPts = makeRoundedRectVerts(x, y, w, h, r, 5);
    std::vector<CCPoint> path;
    path.reserve(80);
    size_t nCorners = cornerPts.size();
    for (size_t i = 0; i < nCorners; ++i) {
        CCPoint p1 = cornerPts[i];
        CCPoint p2 = cornerPts[(i + 1) % nCorners];
        float dist = std::hypot(p2.x - p1.x, p2.y - p1.y);
        int steps = std::max(1, static_cast<int>(dist / ps(18.f)));
        for (int s = 0; s < steps; ++s) {
            float t = static_cast<float>(s) / static_cast<float>(steps);
            path.push_back(ccp(p1.x + (p2.x - p1.x) * t, p1.y + (p2.y - p1.y) * t));
        }
    }

    int totalSegs = static_cast<int>(path.size());
    if (totalSegs < 4) return;

    ccColor4F baseCol4F = rgba4F(baseBorderCol, wormEnabled ? 0.42f : 0.92f);
    for (int i = 0; i < totalSegs; ++i) {
        CCPoint a = path[i];
        CCPoint b = path[(i + 1) % totalSegs];
        dn->drawSegment(a, b, lineThickness * 0.5f, baseCol4F);
    }

    if (!wormEnabled) return;

    int wormLen = std::max(8, totalSegs / 3);
    float headPos = std::fmod(travelPhase * static_cast<float>(totalSegs), static_cast<float>(totalSegs));
    if (headPos < 0.f) headPos += static_cast<float>(totalSegs);
    int headIdx = static_cast<int>(headPos);

    float baseH = 0.f, baseS = 0.f, baseV = 0.f;
    rgbToHsv(baseBorderCol, baseH, baseS, baseV);

    for (int k = 0; k < wormLen; ++k) {
        int segIdx = (headIdx - k + totalSegs) % totalSegs;
        CCPoint a = path[segIdx];
        CCPoint b = path[(segIdx + 1) % totalSegs];

        float progress = 1.0f - (static_cast<float>(k) / static_cast<float>(wormLen));
        ccColor3B segRgb;
        if (rainbowHue) {
            float hHue = std::fmod(baseH + travelPhase * 0.18f + progress * 0.16f, 1.0f);
            segRgb = hsvToRgbColor(hHue, 0.85f, 1.0f);
        } else {
            float hShift = baseH + std::sin(progress * 3.14159f) * 0.06f;
            ccColor3B shifted = hsvToRgbColor(hShift, std::max(0.45f, baseS), 1.0f);
            segRgb = lerpColor(shifted, COL_TEXT_WHITE, progress * 0.52f);
        }

        float segAlpha = 0.25f + 0.75f * progress;
        float segRadius = lineThickness * (0.48f + 0.28f * progress);
        dn->drawSegment(a, b, segRadius, rgba4F(segRgb, segAlpha));
    }
}

inline CCDrawNode* addRefRoundedBox(
    CCNode* parent,
    float rx, float ry_top, float rw, float rh, float r_ref,
    ccColor3B fillCol, float fillAlpha,
    ccColor3B borderCol, float borderAlpha,
    float borderW_ref = 1.6f, int z = 2
) {
    auto dn = CCDrawNode::create();
    float x = px(rx);
    float y = py(ry_top + rh);
    float w = ps(rw);
    float h = ps(rh);
    float r = ps(r_ref);
    float bw = ps(borderW_ref);

    drawRoundedRect(
        dn, x, y, w, h, r,
        rgba4F(fillCol, fillAlpha),
        bw,
        rgba4F(borderCol, borderAlpha)
    );
    parent->addChild(dn, z);
    return dn;
}

inline void drawSparkle4(
    CCDrawNode* dn, CCPoint center, float radius, ccColor4F col
) {
    float rOuter = radius;
    float rInner = radius * 0.28f;
    CCPoint v1[4] = {
        ccp(center.x, center.y + rOuter),
        ccp(center.x - rInner, center.y),
        ccp(center.x, center.y - rOuter),
        ccp(center.x + rInner, center.y)
    };
    CCPoint v2[4] = {
        ccp(center.x - rOuter, center.y),
        ccp(center.x, center.y - rInner),
        ccp(center.x + rOuter, center.y),
        ccp(center.x, center.y + rInner)
    };
    dn->drawPolygon(v1, 4, col, 0.f, col);
    dn->drawPolygon(v2, 4, col, 0.f, col);
}

inline void drawCircleShape(
    CCDrawNode* dn, CCPoint center, float radius,
    ccColor4F fill, float borderW, ccColor4F border, int segs = 24
) {
    std::vector<CCPoint> pts;
    pts.reserve(static_cast<size_t>(segs));
    const float twoPi = 6.2831853f;
    for (int i = 0; i < segs; ++i) {
        float a = twoPi * static_cast<float>(i) / static_cast<float>(segs);
        pts.push_back(ccp(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius));
    }
    dn->drawPolygon(pts.data(), static_cast<unsigned int>(pts.size()), fill, borderW, border);
}

// Dibuja los dos picos minimalistas en la esquina inferior izquierda de la barra lateral (fiel al PDF)
inline void redrawSpikesWormRgb(CCDrawNode* dn, float travelPhase, bool rgbEnabled) {
    if (!dn) return;
    dn->clear();

    auto pal = ThemeConfig::getPalette();
    ccColor3B spikeCol = ThemeConfig::getSpikesRgbColor();
    ccColor3B spikeSoft = lerpColor(spikeCol, pal.accentSoft, 0.45f);
    ccColor4F spikeFill = rgba4F(spikeCol, 0.16f);

    // Coordenadas exactas dentro del pie de la barra lateral (x_ref=44..220, y_top_ref=772..848)
    CCPoint spike1[3] = {
        ccp(px(58.f),  py(844.f)),
        ccp(px(98.f),  py(778.f)),
        ccp(px(138.f), py(844.f))
    };
    dn->drawPolygon(spike1, 3, spikeFill, ps(1.1f), rgba4F(spikeCol, 0.65f));

    CCPoint spike2[3] = {
        ccp(px(124.f), py(844.f)),
        ccp(px(158.f), py(798.f)),
        ccp(px(192.f), py(844.f))
    };
    dn->drawPolygon(spike2, 3, spikeFill, ps(1.1f), rgba4F(spikeCol, 0.65f));

    // Linea base horizontal debajo de los picos
    dn->drawSegment(
        ccp(px(46.f), py(844.f)),
        ccp(px(218.f), py(844.f)),
        ps(1.1f),
        rgba4F(spikeCol, 0.55f)
    );

    // Trazos de enfasis luminoso en las aristas superiores (como en picos.svg del PDF)
    dn->drawSegment(ccp(px(74.f), py(818.f)), ccp(px(98.f), py(778.f)), ps(1.6f), rgba4F(spikeSoft, 0.95f));
    dn->drawSegment(ccp(px(98.f), py(778.f)), ccp(px(106.f), py(791.f)), ps(1.6f), rgba4F(spikeSoft, 0.95f));
    dn->drawSegment(ccp(px(138.f), py(825.f)), ccp(px(158.f), py(798.f)), ps(1.6f), rgba4F(spikeSoft, 0.95f));
    dn->drawSegment(ccp(px(158.f), py(798.f)), ccp(px(166.f), py(809.f)), ps(1.6f), rgba4F(spikeSoft, 0.95f));

    if (!rgbEnabled) return;

    // Estela de luz que recorre los picos de izquierda a derecha
    CCPoint polyPath[7] = {
        ccp(px(46.f),  py(844.f)),
        ccp(px(58.f),  py(844.f)),
        ccp(px(98.f),  py(778.f)),
        ccp(px(131.f), py(833.f)),
        ccp(px(158.f), py(798.f)),
        ccp(px(192.f), py(844.f)),
        ccp(px(218.f), py(844.f))
    };
    std::vector<CCPoint> segs;
    for (int i = 0; i < 6; ++i) {
        CCPoint p1 = polyPath[i], p2 = polyPath[i + 1];
        for (int s = 0; s < 6; ++s) {
            float t = static_cast<float>(s) / 6.f;
            segs.push_back(ccp(p1.x + (p2.x - p1.x) * t, p1.y + (p2.y - p1.y) * t));
        }
    }
    segs.push_back(polyPath[6]);
    int n = static_cast<int>(segs.size());
    int wormLen = 10;
    int head = static_cast<int>(std::fmod(travelPhase * static_cast<float>(n), static_cast<float>(n)));
    if (head < 0) head += n;
    for (int k = 0; k < wormLen; ++k) {
        int idx = (head - k + n) % (n - 1);
        float prog = 1.f - static_cast<float>(k) / static_cast<float>(wormLen);
        ccColor3B c = lerpColor(spikeCol, COL_TEXT_WHITE, prog * 0.65f);
        dn->drawSegment(segs[idx], segs[idx + 1], ps(1.3f + 0.9f * prog), rgba4F(c, 0.25f + 0.75f * prog));
    }
}

inline CCSprite* createFitSprite(char const* name, float w, float h, CCPoint pos) {
    auto spr = CCSprite::create(name);
    if (!spr) return nullptr;
    float s = std::min(w / spr->getContentWidth(), h / spr->getContentHeight());
    spr->setScale(s);
    spr->setPosition(pos);
    return spr;
}

// Boton circular "<" de la esquina superior izquierda (fiel a las Paginas 1-5 del PDF)
inline CCNode* createBackArrowIcon() {
    auto pal = ThemeConfig::getPalette();
    float d = ps(46.f);
    auto node = CCNode::create();
    node->setContentSize({d, d});

    auto dn = CCDrawNode::create();
    drawCircleShape(
        dn, ccp(d * 0.5f, d * 0.5f), d * 0.5f - ps(1.f),
        rgba4F(pal.card, 0.98f), ps(1.2f), rgba4F(pal.line, 0.95f), 28
    );
    node->addChild(dn, 1);

    if (auto spr = createFitSprite("hd-back.png"_spr, ps(18.f), ps(18.f), ccp(d * 0.48f, d * 0.5f))) {
        spr->setColor(pal.text2);
        node->addChild(spr, 2);
    } else {
        dn->drawSegment(ccp(d * 0.56f, d * 0.66f), ccp(d * 0.38f, d * 0.50f), ps(1.8f), rgba4F(pal.text2, 1.f));
        dn->drawSegment(ccp(d * 0.38f, d * 0.50f), ccp(d * 0.56f, d * 0.34f), ps(1.8f), rgba4F(pal.text2, 1.f));
    }
    return node;
}

inline void applyRobotColorToNode(CCNode* robotNode, ccColor3B robotCol) {
    if (!robotNode) return;
    auto pal = ThemeConfig::getPalette();
    if (auto bgDn = typeinfo_cast<CCDrawNode*>(robotNode->getChildByTag(300))) {
        bgDn->clear();
        CCSize sz = robotNode->getContentSize();
        drawRoundedRect(
            bgDn, sz.width * 0.16f, sz.height * 0.08f, sz.width * 0.66f, sz.height * 0.66f, ps(18.f),
            rgba4F(pal.bgOuter, 1.f), 0.f, ccc4f(0, 0, 0, 0)
        );
    }
    if (auto spr = typeinfo_cast<CCSprite*>(robotNode->getChildByTag(301))) {
        spr->setColor(robotCol);
    }
}

inline CCNode* createRobotMascotNode(bool isHeaderMascot) {
    float w = isHeaderMascot ? ps(118.f) : ps(44.f);
    float h = isHeaderMascot ? ps(96.f)  : ps(38.f);
    auto node = CCNode::create();
    node->setContentSize({w, h});

    auto pal = ThemeConfig::getPalette();
    ccColor3B robotCol = ThemeConfig::getRobotRgbColor();

    // Mascara oscura detras de la pantalla del robot para cortar limpiamente la linea superior
    auto bgDn = CCDrawNode::create();
    bgDn->setTag(300);
    drawRoundedRect(
        bgDn, w * 0.16f, h * 0.08f, w * 0.66f, h * 0.66f, ps(isHeaderMascot ? 18.f : 8.f),
        rgba4F(pal.bgOuter, 1.f), 0.f, ccc4f(0, 0, 0, 0)
    );
    node->addChild(bgDn, 1);

    char const* sprName = isHeaderMascot ? "decomind-mascot.png"_spr : "decomind-icon.png"_spr;
    if (auto spr = CCSprite::create(sprName)) {
        spr->setTag(301);
        float sx = w / spr->getContentWidth();
        float sy = h / spr->getContentHeight();
        spr->setScale(std::min(sx, sy));
        spr->setPosition({w * 0.5f, h * 0.5f});
        spr->setColor(robotCol);
        node->addChild(spr, 2);
        return node;
    }

    drawRoundedRect(
        bgDn, w * 0.15f, h * 0.12f, w * 0.70f, h * 0.68f, ps(14.f),
        rgba4F(pal.bgOuter, 1.f), ps(2.5f), rgba4F(robotCol, 1.f)
    );
    return node;
}

//Crea una etiqueta con la fuente seleccionada actualmente en Ajustes (con fallback seguro)
inline CCLabelBMFont* createStyledLabel(
    std::string const& text,
    bool isTitle,
    float baseScale,
    ccColor3B color,
    float maxWidth = 0.f
) {
    std::string formatted = ThemeConfig::formatTextForActiveFont(text);
    char const* fnt = ThemeConfig::getActiveFontFile(isTitle);
    auto lbl = CCLabelBMFont::create(formatted.c_str(), fnt);
    if (!lbl) {
        lbl = CCLabelBMFont::create(formatted.c_str(), isTitle ? "bigFont.fnt" : "chatFont.fnt");
    }
    if (!lbl) return nullptr;
    float finalScale = baseScale * ThemeConfig::getActiveFontScaleMul(isTitle);
    lbl->setColor(color);
    lbl->setScale(finalScale);
    if (maxWidth > 0.f) {
        lbl->limitLabelWidth(maxWidth, finalScale, 0.10f);
    }
    return lbl;
}

// Botones de la barra lateral fieles al PDF:
// - Activo: pildora en color Acento (92/64) con icono y texto en Tinta sobre acento (70/8)
// - Inactivo: fondo transparente con icono y texto en Texto 2 (20/76)
inline CCNode* createSidebarButtonNode(int idx, char const* title, bool active) {
    auto pal = ThemeConfig::getPalette();
    float w = ps(186.f), h = ps(58.f);
    auto node = CCNode::create();
    node->setContentSize({w, h});

    if (active) {
        auto dn = CCDrawNode::create();
        drawRoundedRect(
            dn, 0.f, 0.f, w, h, ps(13.f),
            rgba4F(pal.accent, 1.f),
            0.f,
            ccc4f(0, 0, 0, 0)
        );
        node->addChild(dn, 1);
    }

    static char const* const SB_ICONS[5] = {
        "sb-home.png"_spr, "sb-modes.png"_spr, "sb-chat.png"_spr,
        "sb-stats.png"_spr, "sb-settings.png"_spr
    };
    int sbIdx = (idx >= 0 && idx < 5) ? idx : 0;
    ccColor3B contentCol = active ? pal.inkOnAccent : pal.text2;

    if (auto ico = createFitSprite(SB_ICONS[sbIdx], ps(24.f), ps(24.f), ccp(ps(28.f), h * 0.5f))) {
        ico->setColor(contentCol);
        node->addChild(ico, 2);
    }

    if (auto lbl = createStyledLabel(title, true, 0.25f, contentCol, w - ps(58.f))) {
        lbl->setAnchorPoint({0.f, 0.5f});
        lbl->setPosition({ps(50.f), h * 0.5f});
        node->addChild(lbl, 2);
    }

    return node;
}

// Icono del modo dentro de su recuadro redondeado (como en las tarjetas de Pagina 1 y 2)
inline CCNode* createModeIconBadge(int modeIdx, bool selected, float boxSizeRef = 48.f, float radiusRef = 12.f) {
    auto pal = ThemeConfig::getPalette();
    float sz = ps(boxSizeRef);
    auto holder = CCNode::create();
    holder->setContentSize({sz, sz});

    int safeIdx = (modeIdx >= 0 && modeIdx < 9) ? modeIdx : 0;
    ccColor3B badgeFill = selected ? pal.accent : pal.modeBadgeFill(safeIdx);
    ccColor3B badgeBord = selected ? pal.accentSoft : pal.line;
    ccColor3B iconCol   = selected ? pal.inkOnAccent : pal.modeAccentSoft(safeIdx);

    auto dn = CCDrawNode::create();
    drawRoundedRect(
        dn, 0.f, 0.f, sz, sz, ps(radiusRef),
        rgba4F(badgeFill, 1.f),
        ps(1.0f),
        rgba4F(badgeBord, selected ? 0.95f : 0.65f)
    );
    holder->addChild(dn, 1);

    if (auto spr = createFitSprite(MODES[safeIdx].spriteName, sz * 0.52f, sz * 0.52f, ccp(sz * 0.5f, sz * 0.5f))) {
        spr->setColor(iconCol);
        holder->addChild(spr, 2);
    }
    return holder;
}

inline CCSprite* createBottomBarIconSprite(int idx, CCPoint pos) {
    auto pal = ThemeConfig::getPalette();
    static char const* const BB_ICONS[5] = {
        "bb-fav.png"_spr, "bb-tutorials.png"_spr, "bb-levels.png"_spr,
        "bb-save.png"_spr, "bb-reset.png"_spr
    };
    int i = (idx >= 0 && idx < 5) ? idx : 0;
    auto spr = createFitSprite(BB_ICONS[i], ps(22.f), ps(22.f), pos);
    if (spr) spr->setColor(pal.accentSoft);
    return spr;
}

// Boton plano con soporte de texto oscuro sobre acento (Regla 2 de la Pagina 6)
inline CCNode* makeCustomMenuBtn(
    char const* labelText, float w, float h,
    ccColor3B fillCol, ccColor3B borderCol, float fontScale = 0.26f,
    bool useDarkInk = false, float cornerRadius = 5.f
) {
    auto pal = ThemeConfig::getPalette();
    auto node = CCNode::create();
    node->setContentSize({w, h});

    auto dn = CCDrawNode::create();
    drawRoundedRect(
        dn, 0.f, 0.f, w, h, cornerRadius,
        rgba4F(fillCol, 0.98f),
        useDarkInk ? 0.f : 0.9f,
        rgba4F(borderCol, 0.85f)
    );
    node->addChild(dn, 1);

    ccColor3B txtCol = useDarkInk ? pal.inkOnAccent : pal.text;
    if (auto lbl = createStyledLabel(labelText, true, fontScale, txtCol, w - 10.f)) {
        lbl->setPosition({w * 0.5f, h * 0.5f});
        node->addChild(lbl, 2);
    }
    return node;
}

} // namespace ui
} // namespace decomind
