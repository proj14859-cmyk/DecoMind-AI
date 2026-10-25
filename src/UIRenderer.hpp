#pragma once
// ===========================================================================
// DecoMind AI v0.3.2-alpha - Motor de Renderizado Visual Nativo (Cocos2d-x)
//
// Parche y modificacion del diseno v0.3.2-alpha:
//   - Tarjetas de modos en color entero limpio (sin degradado escalonado).
//   - Botones laterales y de accion en color entero con iluminacion media.
//   - Contorno RGB tipo "gusano" (flujo luminoso trasladandose de A hacia B
//     alrededor del perimetro) con velocidad configurable.
//   - Eliminados el rombo y la barra de progreso de la esquina superior derecha.
// ===========================================================================
#include "Modes.hpp"
#include <algorithm>
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

// Paleta base por defecto
static const ccColor3B COL_BG_OUTER      = ccc3(0x00, 0x09, 0x3d);
static const ccColor3B COL_BG_FRAME      = ccc3(0x00, 0x0b, 0x32);
static const ccColor3B COL_BG_CENTER     = ccc3(0x00, 0x0d, 0x39);
static const ccColor3B COL_BG_RIGHT_CARD = ccc3(0x00, 0x10, 0x3c);
static const ccColor3B COL_BG_INPUT      = ccc3(0x04, 0x13, 0x31);
static const ccColor3B COL_BG_SIDE_IDLE  = ccc3(0x05, 0x19, 0x4c);
// Iluminacion media y color entero para botones seleccionados (sin brillo exagerado)
static const ccColor3B COL_BG_SIDE_ACT   = ccc3(0x0d, 0x4f, 0xa8);
static const ccColor3B COL_BG_BOTTOM     = ccc3(0x00, 0x0a, 0x2f);
static const ccColor3B COL_BG_ALLY       = ccc3(0x01, 0x0e, 0x48);

static const ccColor3B COL_FRAME_BLUE    = ccc3(0x0a, 0x4c, 0xc4);
static const ccColor3B COL_FRAME_CYAN    = ccc3(0x18, 0xc8, 0xff);
static const ccColor3B COL_NEON_GREEN    = ccc3(0x1e, 0xf0, 0x98);
static const ccColor3B COL_BAR_TEAL      = ccc3(0x28, 0xf2, 0xba);
static const ccColor3B COL_TEXT_WHITE    = ccc3(0xff, 0xff, 0xff);
static const ccColor3B COL_TEXT_CYAN     = ccc3(0x70, 0xc8, 0xff);
static const ccColor3B COL_TEXT_SOFT     = ccc3(0x8c, 0xb2, 0xe8);
static const ccColor3B COL_TEXT_DIM      = ccc3(0x56, 0x79, 0xb5);
static const ccColor3B COL_DIVIDER       = ccc3(0x12, 0x32, 0x74);

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

// Configuracion persistente de colores y flujo RGB
struct ThemeConfig {
    static ccColor3B getMenuAccent() {
        int r = Mod::get()->getSavedValue<int>("theme_r", static_cast<int>(COL_FRAME_BLUE.r));
        int g = Mod::get()->getSavedValue<int>("theme_g", static_cast<int>(COL_FRAME_BLUE.g));
        int b = Mod::get()->getSavedValue<int>("theme_b", static_cast<int>(COL_FRAME_BLUE.b));
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
    }

    static bool isRgbEnabled() {
        return Mod::get()->getSavedValue<bool>("rgb_enabled", true);
    }

    static void setRgbEnabled(bool en) {
        Mod::get()->setSavedValue<bool>("rgb_enabled", en);
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
    }

    static ccColor3B getClipboardColor() {
        int r = Mod::get()->getSavedValue<int>("clip_col_r", 10);
        int g = Mod::get()->getSavedValue<int>("clip_col_g", 76);
        int b = Mod::get()->getSavedValue<int>("clip_col_b", 196);
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

// Dibuja un contorno con efecto "gusano" (flujo RGB que se traslada del punto A al punto B
// recorriendo el perimetro del recuadro redondeado).
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

    // Subdividimos los lados rectos y esquinas en puntos equidistantes alrededor del perimetro
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

    // 1. Contorno base tenue estatico para mantener la silueta del recuadro
    ccColor4F baseCol4F = rgba4F(baseBorderCol, wormEnabled ? 0.35f : 0.92f);
    for (int i = 0; i < totalSegs; ++i) {
        CCPoint a = path[i];
        CCPoint b = path[(i + 1) % totalSegs];
        dn->drawSegment(a, b, lineThickness * 0.5f, baseCol4F);
    }

    if (!wormEnabled) return;

    // 2. Gusano luminoso trasladandose de A hacia B a lo largo del perimetro
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

        // Progreso desde la cola (0.0) hasta la cabeza del gusano (1.0)
        float progress = 1.0f - (static_cast<float>(k) / static_cast<float>(wormLen));
        ccColor3B segRgb;
        if (rainbowHue) {
            float hHue = std::fmod(travelPhase * 0.25f + progress * 0.35f, 1.0f);
            segRgb = hsvToRgbColor(hHue, 0.85f, 1.0f);
        } else {
            // Flujo dentro de la paleta del boton (va desde el color base hacia un tono brillante/blanco en la punta)
            float hShift = baseH + std::sin(progress * 3.14159f) * 0.06f;
            ccColor3B shifted = hsvToRgbColor(hShift, std::max(0.5f, baseS), 1.0f);
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
    float borderW_ref = 2.2f, int z = 2
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
    dn->drawDot(center, rInner * 1.1f, ccc4f(1.f, 1.f, 1.f, col.a));
}

inline void drawCircleShape(
    CCDrawNode* dn, CCPoint center, float radius,
    ccColor4F fill, float borderW, ccColor4F border, int segs = 20
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

// Dibuja los picos decorativos de Geometry Dash con un flujo RGB que se traslada de izquierda a derecha
inline void redrawSpikesWormRgb(CCDrawNode* dn, float travelPhase, bool rgbEnabled) {
    if (!dn) return;
    dn->clear();

    ccColor3B baseCyan = COL_FRAME_CYAN;
    ccColor3B baseBlue = COL_FRAME_BLUE;
    ccColor4F spikeFill = rgba4F(ccc3(0x03, 0x22, 0x62), 0.95f);
    ccColor4F spikeInner = rgba4F(ccc3(0x0c, 0x52, 0xb8), 0.75f);

    // Relleno base de los dos picos
    CCPoint spike1[3] = {
        ccp(px(68.f), py(872.f)),
        ccp(px(111.f), py(786.f)),
        ccp(px(154.f), py(872.f))
    };
    dn->drawPolygon(spike1, 3, spikeFill, ps(2.0f), rgba4F(baseBlue, 0.65f));
    CCPoint spike1In[3] = {
        ccp(px(86.f), py(870.f)),
        ccp(px(111.f), py(812.f)),
        ccp(px(136.f), py(870.f))
    };
    dn->drawPolygon(spike1In, 3, spikeInner, ps(1.0f), rgba4F(baseBlue, 0.7f));

    CCPoint spike2[3] = {
        ccp(px(146.f), py(872.f)),
        ccp(px(176.f), py(816.f)),
        ccp(px(206.f), py(872.f))
    };
    dn->drawPolygon(spike2, 3, spikeFill, ps(1.8f), rgba4F(baseBlue, 0.65f));
    CCPoint spike2In[3] = {
        ccp(px(160.f), py(870.f)),
        ccp(px(176.f), py(836.f)),
        ccp(px(192.f), py(870.f))
    };
    dn->drawPolygon(spike2In, 3, spikeInner, ps(1.0f), rgba4F(baseBlue, 0.7f));

    // Ruta continua A -> B sobre las lineas y contornos de los picos
    CCPoint polyPath[9] = {
        ccp(px(40.f), py(736.f)),
        ccp(px(40.f), py(874.f)),
        ccp(px(68.f), py(872.f)),
        ccp(px(111.f), py(786.f)),
        ccp(px(150.f), py(872.f)),
        ccp(px(176.f), py(816.f)),
        ccp(px(206.f), py(872.f)),
        ccp(px(252.f), py(874.f)),
        ccp(px(68.f), py(874.f))
    };

    std::vector<CCPoint> segs;
    for (int i = 0; i < 8; ++i) {
        CCPoint p1 = polyPath[i], p2 = polyPath[i + 1];
        int steps = 6;
        for (int s = 0; s < steps; ++s) {
            float t = static_cast<float>(s) / static_cast<float>(steps);
            segs.push_back(ccp(p1.x + (p2.x - p1.x) * t, p1.y + (p2.y - p1.y) * t));
        }
    }
    int n = static_cast<int>(segs.size());
    for (int i = 0; i + 1 < n; ++i) {
        dn->drawSegment(segs[i], segs[i + 1], ps(1.4f), rgba4F(baseCyan, rgbEnabled ? 0.35f : 0.9f));
    }

    if (!rgbEnabled) return;

    int wormLen = 16;
    int head = static_cast<int>(std::fmod(travelPhase * static_cast<float>(n), static_cast<float>(n)));
    if (head < 0) head += n;
    for (int k = 0; k < wormLen; ++k) {
        int idx = (head - k + n) % (n - 1);
        float prog = 1.f - static_cast<float>(k) / static_cast<float>(wormLen);
        ccColor3B c = hsvToRgbColor(std::fmod(travelPhase * 0.25f + prog * 0.3f, 1.f), 0.85f, 1.f);
        dn->drawSegment(segs[idx], segs[idx + 1], ps(1.4f + 1.0f * prog), rgba4F(c, 0.3f + 0.7f * prog));
    }
}

inline void drawSciFiMainFrame(CCDrawNode* dn, ccColor3B accentCol) {
    if (!dn) return;
    dn->clear();

    ccColor3B outerBg = lerpColor(COL_BG_OUTER, accentCol, 0.10f);
    ccColor3B frameBg = lerpColor(COL_BG_FRAME, accentCol, 0.12f);
    ccColor3B topEdge = lerpColor(accentCol, COL_TEXT_WHITE, 0.30f);

    drawRoundedRect(
        dn, 0.f, 0.f, PW, PH, ps(12.f),
        rgba4F(outerBg, 0.98f),
        ps(2.6f), rgba4F(accentCol, 0.9f)
    );

    ccColor4F bgGrid = rgba4F(lerpColor(outerBg, accentCol, 0.35f), 0.45f);
    dn->drawSegment(ccp(px(260.f), py(42.f)), ccp(px(460.f), py(42.f)), ps(2.f), bgGrid);
    dn->drawSegment(ccp(px(460.f), py(42.f)), ccp(px(490.f), py(88.f)), ps(2.f), bgGrid);
    dn->drawSegment(ccp(px(490.f), py(88.f)), ccp(px(690.f), py(88.f)), ps(2.f), bgGrid);

    float xL = px(14.f), xR = px(1522.f);
    float yT = py(120.f), yB = py(882.f);
    float ch = ps(18.f);
    CCPoint framePts[8] = {
        ccp(xL + ch, yT),
        ccp(xR - ch, yT),
        ccp(xR, yT - ch),
        ccp(xR, yB + ch),
        ccp(xR - ch, yB),
        ccp(xL + ch, yB),
        ccp(xL, yB + ch),
        ccp(xL, yT - ch)
    };
    dn->drawPolygon(
        framePts, 8,
        rgba4F(frameBg, 0.96f),
        ps(3.0f),
        rgba4F(accentCol, 1.f)
    );

    dn->drawSegment(ccp(xL + ch, yT), ccp(xR - ch, yT), ps(2.0f), rgba4F(topEdge, 0.85f));
    dn->drawSegment(ccp(xL + ch, yB), ccp(xR - ch, yB), ps(1.8f), rgba4F(accentCol, 0.95f));

    ccColor4F blueLine = rgba4F(accentCol, 0.95f);
    dn->drawSegment(ccp(px(18.f), py(892.f)), ccp(px(962.f), py(892.f)), ps(2.2f), blueLine);
    dn->drawSegment(ccp(px(962.f), py(892.f)), ccp(px(995.f), py(982.f)), ps(2.2f), blueLine);
    dn->drawSegment(ccp(px(18.f), py(982.f)), ccp(px(1485.f), py(982.f)), ps(2.2f), blueLine);
}

inline CCSprite* createFitSprite(char const* name, float w, float h, CCPoint pos) {
    auto spr = CCSprite::create(name);
    if (!spr) return nullptr;
    float s = std::min(w / spr->getContentWidth(), h / spr->getContentHeight());
    spr->setScale(s);
    spr->setPosition(pos);
    return spr;
}

inline CCNode* createBackArrowIcon() {
    float w = ps(86.f), h = ps(88.f);
    auto node = CCNode::create();
    node->setContentSize({w, h});

    if (auto spr = createFitSprite("hd-back.png"_spr, w, h, ccp(w * 0.5f, h * 0.5f))) {
        node->addChild(spr, 2);
        return node;
    }

    auto dn = CCDrawNode::create();
    CCPoint outerTri[3] = {
        ccp(w * 0.12f, h * 0.50f),
        ccp(w * 0.84f, h * 0.90f),
        ccp(w * 0.84f, h * 0.10f)
    };
    dn->drawPolygon(
        outerTri, 3,
        rgba4F(ccc3(0x07, 0x38, 0x3c), 0.98f),
        ps(4.2f),
        rgba4F(ccc3(0x72, 0xff, 0xd8), 1.f)
    );

    CCPoint innerTri[3] = {
        ccp(w * 0.24f, h * 0.50f),
        ccp(w * 0.76f, h * 0.79f),
        ccp(w * 0.76f, h * 0.21f)
    };
    dn->drawPolygon(
        innerTri, 3,
        rgba4F(COL_NEON_GREEN, 1.f),
        ps(2.0f),
        rgba4F(ccc3(0xa8, 0xff, 0xe4), 1.f)
    );
    node->addChild(dn);
    return node;
}

inline void drawGearShape(CCDrawNode* dn, CCPoint c, float r, ccColor4F col, ccColor4F holeCol) {
    const int teeth = 8;
    const float twoPi = 6.2831853f;
    for (int i = 0; i < teeth; ++i) {
        float a = twoPi * static_cast<float>(i) / static_cast<float>(teeth);
        CCPoint p1 = ccp(c.x + std::cos(a) * (r * 0.55f), c.y + std::sin(a) * (r * 0.55f));
        CCPoint p2 = ccp(c.x + std::cos(a) * r, c.y + std::sin(a) * r);
        dn->drawSegment(p1, p2, r * 0.24f, col);
    }
    drawCircleShape(dn, c, r * 0.76f, col, 0.f, col, 18);
    drawCircleShape(dn, c, r * 0.36f, holeCol, 0.f, holeCol, 14);
}

inline CCNode* createRobotMascotNode(bool isHeaderMascot) {
    float w = isHeaderMascot ? ps(220.f) : ps(72.f);
    float h = isHeaderMascot ? ps(140.f) : ps(54.f);
    auto node = CCNode::create();
    node->setContentSize({w, h});

    char const* sprName = isHeaderMascot ? "decomind-mascot.png"_spr : "decomind-icon.png"_spr;
    if (auto spr = CCSprite::create(sprName)) {
        float sx = w / spr->getContentWidth();
        float sy = h / spr->getContentHeight();
        spr->setScale(std::min(sx, sy));
        spr->setPosition({w * 0.5f, h * 0.5f});
        node->addChild(spr, 2);
        return node;
    }

    auto dn = CCDrawNode::create();
    float cx = w * (isHeaderMascot ? 0.45f : 0.5f);
    float cy = h * 0.48f;
    float rw = isHeaderMascot ? ps(126.f) : ps(52.f);
    float rh = isHeaderMascot ? ps(86.f) : ps(38.f);

    drawRoundedRect(
        dn, cx - rw * 0.5f, cy - rh * 0.5f, rw, rh, ps(18.f),
        rgba4F(ccc3(0x28, 0xd8, 0xff), 1.f), ps(3.5f), rgba4F(ccc3(0x02, 0x12, 0x38), 1.f)
    );
    drawRoundedRect(
        dn, cx - rw * 0.42f, cy - rh * 0.40f, rw * 0.84f, rh * 0.80f, ps(13.f),
        rgba4F(ccc3(0x02, 0x0c, 0x2c), 1.f), ps(2.f), rgba4F(ccc3(0x0a, 0x48, 0x9c), 1.f)
    );
    node->addChild(dn);
    return node;
}

inline void drawSidebarIcon(CCDrawNode* dn, int idx, CCPoint c, bool active) {
    ccColor4F col = active ? rgba4F(COL_TEXT_WHITE, 1.f) : rgba4F(ccc3(0x67, 0x98, 0xf0), 1.f);
    ccColor4F hole = active ? rgba4F(COL_BG_SIDE_ACT, 1.f) : rgba4F(COL_BG_SIDE_IDLE, 1.f);
    float r = ps(20.f);

    switch (idx) {
        case 0: {
            CCPoint roof[3] = {
                ccp(c.x - r * 1.05f, c.y + r * 0.05f),
                ccp(c.x,             c.y + r * 0.95f),
                ccp(c.x + r * 1.05f, c.y + r * 0.05f)
            };
            dn->drawPolygon(roof, 3, col, ps(1.5f), col);
            drawRoundedRect(dn, c.x - r * 0.72f, c.y - r * 0.85f, r * 1.44f, r * 0.95f, ps(3.f), col, 0.f, col);
            drawRoundedRect(dn, c.x - r * 0.22f, c.y - r * 0.85f, r * 0.44f, r * 0.52f, ps(2.f), hole, 0.f, hole);
            break;
        }
        case 1: {
            float sq = r * 0.72f;
            float gap = r * 0.14f;
            drawRoundedRect(dn, c.x - gap - sq, c.y + gap, sq, sq, ps(3.f), col, 0.f, col);
            drawRoundedRect(dn, c.x + gap,      c.y + gap, sq, sq, ps(3.f), col, 0.f, col);
            drawRoundedRect(dn, c.x - gap - sq, c.y - gap - sq, sq, sq, ps(3.f), col, 0.f, col);
            drawRoundedRect(dn, c.x + gap,      c.y - gap - sq, sq, sq, ps(3.f), col, 0.f, col);
            break;
        }
        case 2: {
            drawRoundedRect(dn, c.x - r * 0.95f, c.y - r * 0.55f, r * 1.9f, r * 1.35f, ps(6.f), col, 0.f, col);
            for (int d = -1; d <= 1; ++d) {
                dn->drawDot(ccp(c.x + d * r * 0.42f, c.y + r * 0.12f), ps(2.6f), hole);
            }
            break;
        }
        case 3: {
            float bw = r * 0.42f;
            drawRoundedRect(dn, c.x - r * 0.82f, c.y - r * 0.80f, bw, r * 1.00f, ps(2.f), col, 0.f, col);
            drawRoundedRect(dn, c.x - bw * 0.5f, c.y - r * 0.80f, bw, r * 1.60f, ps(2.f), col, 0.f, col);
            drawRoundedRect(dn, c.x + r * 0.40f, c.y - r * 0.80f, bw, r * 0.72f, ps(2.f), col, 0.f, col);
            break;
        }
        default: {
            drawGearShape(dn, c, r * 0.92f, col, hole);
            break;
        }
    }
}

// Botones de la barra lateral en color entero limpio (sin degradado) e iluminacion media al seleccionarlos
inline CCNode* createSidebarButtonNode(int idx, char const* title, bool active) {
    float w = ps(258.f), h = ps(80.f);
    auto node = CCNode::create();
    node->setContentSize({w, h});

    ccColor3B themeAcc = ThemeConfig::getMenuAccent();
    // Color entero de iluminacion media cuando esta seleccionado
    ccColor3B actFill  = lerpColor(COL_BG_SIDE_ACT, themeAcc, 0.35f);
    ccColor3B actBord  = lerpColor(actFill, COL_FRAME_CYAN, 0.48f);
    ccColor3B idleFill = COL_BG_SIDE_IDLE;
    ccColor3B idleBord = ccc3(0x14, 0x32, 0x6e);

    auto dn = CCDrawNode::create();
    if (active) {
        drawRoundedRect(
            dn, 0.f, 0.f, w, h, ps(12.f),
            rgba4F(actFill, 0.98f),
            ps(2.0f),
            rgba4F(actBord, 0.95f)
        );
    } else {
        drawRoundedRect(
            dn, 0.f, 0.f, w, h, ps(12.f),
            rgba4F(idleFill, 0.95f),
            ps(1.6f),
            rgba4F(idleBord, 0.90f)
        );
    }

    static char const* const SB_ICONS[5] = {
        "sb-home.png"_spr, "sb-modes.png"_spr, "sb-chat.png"_spr,
        "sb-stats.png"_spr, "sb-settings.png"_spr
    };
    int sbIdx = (idx >= 0 && idx < 5) ? idx : 0;
    if (auto ico = createFitSprite(SB_ICONS[sbIdx], ps(42.f), ps(42.f), ccp(ps(52.f), h * 0.5f))) {
        ico->setColor(active ? COL_TEXT_WHITE : ccc3(0x67, 0x98, 0xf0));
        node->addChild(ico, 2);
    } else {
        drawSidebarIcon(dn, idx, ccp(ps(52.f), h * 0.5f), active);
    }
    node->addChild(dn, 1);

    auto lbl = CCLabelBMFont::create(title, "bigFont.fnt");
    lbl->setAnchorPoint({0.f, 0.5f});
    lbl->setPosition({ps(92.f), h * 0.5f});
    lbl->setColor(active ? COL_TEXT_WHITE : COL_TEXT_SOFT);
    lbl->limitLabelWidth(w - ps(104.f), 0.36f, 0.18f);
    node->addChild(lbl, 2);

    return node;
}

inline CCNode* createModeIconNode(int modeIdx) {
    float boxW = ps(96.f), boxH = ps(76.f);
    auto holder = CCNode::create();
    holder->setContentSize({boxW, boxH});

    auto const& m = MODES[modeIdx];
    if (auto spr = CCSprite::create(m.spriteName)) {
        float sx = boxW / spr->getContentWidth();
        float sy = boxH / spr->getContentHeight();
        spr->setScale(std::min(sx, sy));
        spr->setPosition({boxW * 0.5f, boxH * 0.5f});
        holder->addChild(spr, 2);
        return holder;
    }

    auto dn = CCDrawNode::create();
    CCPoint c = ccp(boxW * 0.5f, boxH * 0.5f);
    ccColor4F col = rgba4F(m.borderColor, 1.f);
    ccColor4F bg = rgba4F(m.bgColor, 1.f);
    float r = ps(26.f);
    drawRoundedRect(dn, c.x - r, c.y - r * 0.7f, r * 2.f, r * 1.4f, ps(4.f), bg, ps(2.5f), col);
    holder->addChild(dn);
    return holder;
}

inline void drawBottomBarIcon(CCDrawNode* dn, int idx, CCPoint c) {
    ccColor4F col = rgba4F(ccc3(0x52, 0xa8, 0xf8), 1.f);
    float r = ps(15.f);
    drawCircleShape(dn, c, r * 0.8f, col, 0.f, col, 14);
}

inline CCSprite* createBottomBarIconSprite(int idx, CCPoint pos) {
    static char const* const BB_ICONS[5] = {
        "bb-fav.png"_spr, "bb-tutorials.png"_spr, "bb-levels.png"_spr,
        "bb-save.png"_spr, "bb-reset.png"_spr
    };
    int i = (idx >= 0 && idx < 5) ? idx : 0;
    auto spr = createFitSprite(BB_ICONS[i], ps(36.f), ps(36.f), pos);
    if (spr) spr->setColor(ccc3(0x52, 0xa8, 0xf8));
    return spr;
}

// Boton moderno plano (color entero sin degradado y con borde fino sutil)
inline CCNode* makeCustomMenuBtn(
    char const* labelText, float w, float h,
    ccColor3B fillCol, ccColor3B borderCol, float fontScale = 0.28f
) {
    auto node = CCNode::create();
    node->setContentSize({w, h});

    auto dn = CCDrawNode::create();
    drawRoundedRect(
        dn, 0.f, 0.f, w, h, 4.f,
        rgba4F(fillCol, 0.96f),
        0.9f, rgba4F(borderCol, 0.78f)
    );
    node->addChild(dn, 1);

    auto lbl = CCLabelBMFont::create(sanitizeForGD(labelText).c_str(), "bigFont.fnt");
    lbl->setColor(COL_TEXT_WHITE);
    lbl->setPosition({w * 0.5f, h * 0.5f});
    lbl->limitLabelWidth(w - 10.f, fontScale, 0.14f);
    node->addChild(lbl, 2);
    return node;
}

} // namespace ui
} // namespace decomind
