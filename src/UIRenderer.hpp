#pragma once
// ===========================================================================
// DecoMind AI v0.3.12 - Motor de Renderizado Visual Nativo (Cocos2d-x / Geode)
//
// Reproduce con fidelidad 1:1 la interfaz de referencia (1536x1024 -> 480x320)
// e incorpora soporte para:
//   - Paleta de color personalizable del menu interactivo.
//   - Contornos RGB dinamicos por paleta de color en tarjetas y decoraciones.
//   - Degradados interiores suaves mas claros en las tarjetas de modos.
//   - Carga de sprites HD (_spr) con respaldo vectorial automatico.
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
static const ccColor3B COL_BG_SIDE_ACT   = ccc3(0x00, 0x80, 0xfd);
static const ccColor3B COL_BG_SIDE_ACT_T = ccc3(0x22, 0xb8, 0xff);
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

// Cicla un color dentro de su propia paleta cromatica (ej. naranja -> ambar/coral/dorado)
inline ccColor3B shiftPaletteColor(ccColor3B baseCol, float phase) {
    float h = 0.f, s = 0.f, v = 0.f;
    rgbToHsv(baseCol, h, s, v);
    float wave = std::sin(phase);
    float wave2 = std::cos(phase * 0.7f);
    float hShift = h + wave * 0.055f; // Oscila suavemente dentro de la misma familia de color
    float sShift = std::max(0.45f, std::min(1.f, s + wave2 * 0.12f));
    float vShift = std::max(0.65f, std::min(1.f, v + 0.10f * (0.5f + 0.5f * wave)));
    return hsvToRgbColor(hShift, sShift, vShift);
}

// Configuracion persistente de colores y animacion RGB del menu
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

    static ccColor3B getMenuBgDark() {
        ccColor3B acc = getMenuAccent();
        return lerpColor(COL_BG_FRAME, acc, 0.16f);
    }

    static ccColor3B getMenuCardBg() {
        ccColor3B acc = getMenuAccent();
        return lerpColor(COL_BG_RIGHT_CARD, acc, 0.18f);
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

    static ccColor3B getCustomRgbColor() {
        int r = Mod::get()->getSavedValue<int>("rgb_col_r", static_cast<int>(COL_FRAME_CYAN.r));
        int g = Mod::get()->getSavedValue<int>("rgb_col_g", static_cast<int>(COL_FRAME_CYAN.g));
        int b = Mod::get()->getSavedValue<int>("rgb_col_b", static_cast<int>(COL_FRAME_CYAN.b));
        return ccc3(
            static_cast<GLubyte>(std::clamp(r, 0, 255)),
            static_cast<GLubyte>(std::clamp(g, 0, 255)),
            static_cast<GLubyte>(std::clamp(b, 0, 255))
        );
    }

    static void setCustomRgbColor(ccColor3B c) {
        Mod::get()->setSavedValue<int>("rgb_col_r", c.r);
        Mod::get()->setSavedValue<int>("rgb_col_g", c.g);
        Mod::get()->setSavedValue<int>("rgb_col_b", c.b);
    }

    static bool useCustomRgbBase() {
        return Mod::get()->getSavedValue<bool>("rgb_use_custom", false);
    }

    static void setUseCustomRgbBase(bool u) {
        Mod::get()->setSavedValue<bool>("rgb_use_custom", u);
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

// Dibuja un recuadro con un degradado leve hacia un tono mas claro en la parte superior
inline void drawRoundedRectWithSoftGradient(
    CCDrawNode* dn,
    float x, float y, float w, float h, float r,
    ccColor3B baseCol, ccColor3B accentCol, float alpha,
    float borderW, ccColor4F border
) {
    drawRoundedRect(dn, x, y, w, h, r, rgba4F(baseCol, alpha), 0.f, ccc4f(0, 0, 0, 0));

    // Degradado escalonado suave hacia un tono mas claro de la misma paleta
    ccColor3B midLight = lerpColor(baseCol, accentCol, 0.16f);
    ccColor3B topLight = lerpColor(baseCol, accentCol, 0.30f);
    float inset = ps(3.f);
    drawRoundedRect(
        dn, x + inset, y + h * 0.32f, w - inset * 2.f, h * 0.68f - inset, std::max(1.f, r - inset),
        rgba4F(midLight, 0.42f), 0.f, ccc4f(0, 0, 0, 0)
    );
    drawRoundedRect(
        dn, x + inset, y + h * 0.58f, w - inset * 2.f, h * 0.42f - inset, std::max(1.f, r - inset),
        rgba4F(topLight, 0.38f), 0.f, ccc4f(0, 0, 0, 0)
    );

    if (borderW > 0.f && border.a > 0.01f) {
        drawRoundedRect(dn, x, y, w, h, r, ccc4f(0, 0, 0, 0), borderW, border);
    }
}

inline CCDrawNode* addRefRoundedBox(
    CCNode* parent,
    float rx, float ry_top, float rw, float rh, float r_ref,
    ccColor3B fillCol, float fillAlpha,
    ccColor3B borderCol, float borderAlpha,
    float borderW_ref = 2.5f, int z = 2
) {
    auto dn = CCDrawNode::create();
    float x = px(rx);
    float y = py(ry_top + rh);
    float w = ps(rw);
    float h = ps(rh);
    float r = ps(r_ref);
    float bw = ps(borderW_ref);

    if (borderAlpha > 0.4f && bw > 0.f) {
        drawRoundedRect(
            dn, x - 0.8f, y - 0.8f, w + 1.6f, h + 1.6f, r + 0.8f,
            ccc4f(0.f, 0.f, 0.f, 0.f), bw * 1.15f,
            rgba4F(borderCol, borderAlpha * 0.28f)
        );
    }

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

// Dibuja los picos decorativos de Geometry Dash en un CCDrawNode dedicado para animarlos en RGB
inline void redrawSpikesRgb(CCDrawNode* dn, ccColor3B primaryCol, ccColor3B secondaryCol) {
    if (!dn) return;
    dn->clear();

    ccColor4F cyanGlow = rgba4F(primaryCol, 0.96f);
    ccColor4F blueLine = rgba4F(secondaryCol, 0.95f);
    ccColor3B darkFill = lerpColor(ccc3(0x02, 0x10, 0x34), primaryCol, 0.22f);
    ccColor3B midFill  = lerpColor(ccc3(0x04, 0x1c, 0x52), secondaryCol, 0.42f);
    ccColor4F spikeFill = rgba4F(darkFill, 0.95f);
    ccColor4F spikeInner = rgba4F(midFill, 0.85f);

    // Linea vertical izquierda y pedestal horizontal
    dn->drawSegment(ccp(px(40.f), py(736.f)), ccp(px(40.f), py(874.f)), ps(3.2f), blueLine);
    dn->drawSegment(ccp(px(40.f), py(874.f)), ccp(px(252.f), py(874.f)), ps(4.2f), cyanGlow);

    // Pico grande izquierdo
    CCPoint spike1[3] = {
        ccp(px(68.f), py(872.f)),
        ccp(px(111.f), py(786.f)),
        ccp(px(154.f), py(872.f))
    };
    dn->drawPolygon(spike1, 3, spikeFill, ps(2.8f), cyanGlow);
    CCPoint spike1In[3] = {
        ccp(px(86.f), py(870.f)),
        ccp(px(111.f), py(812.f)),
        ccp(px(136.f), py(870.f))
    };
    dn->drawPolygon(spike1In, 3, spikeInner, ps(1.2f), blueLine);

    // Pico mediano derecho
    CCPoint spike2[3] = {
        ccp(px(146.f), py(872.f)),
        ccp(px(176.f), py(816.f)),
        ccp(px(206.f), py(872.f))
    };
    dn->drawPolygon(spike2, 3, spikeFill, ps(2.5f), cyanGlow);
    CCPoint spike2In[3] = {
        ccp(px(160.f), py(870.f)),
        ccp(px(176.f), py(836.f)),
        ccp(px(192.f), py(870.f))
    };
    dn->drawPolygon(spike2In, 3, spikeInner, ps(1.0f), blueLine);
}

// Dibuja el marco exterior sci-fi usando el color de acento configurado en el tema
inline void drawSciFiMainFrame(CCDrawNode* dn, ccColor3B accentCol) {
    if (!dn) return;
    dn->clear();

    ccColor3B outerBg = lerpColor(COL_BG_OUTER, accentCol, 0.12f);
    ccColor3B frameBg = lerpColor(COL_BG_FRAME, accentCol, 0.14f);
    ccColor3B topEdge = lerpColor(accentCol, COL_TEXT_WHITE, 0.35f);

    drawRoundedRect(
        dn, 0.f, 0.f, PW, PH, ps(12.f),
        rgba4F(outerBg, 0.98f),
        ps(3.f), rgba4F(accentCol, 0.9f)
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
        ps(3.5f),
        rgba4F(accentCol, 1.f)
    );

    dn->drawSegment(ccp(xL + ch, yT), ccp(xR - ch, yT), ps(2.2f), rgba4F(topEdge, 0.85f));
    dn->drawSegment(ccp(xL + ch, yB), ccp(xR - ch, yB), ps(2.0f), rgba4F(accentCol, 0.95f));

    ccColor4F blueLine = rgba4F(accentCol, 0.95f);
    dn->drawSegment(ccp(px(18.f), py(892.f)), ccp(px(962.f), py(892.f)), ps(2.5f), blueLine);
    dn->drawSegment(ccp(px(962.f), py(892.f)), ccp(px(995.f), py(982.f)), ps(2.5f), blueLine);
    dn->drawSegment(ccp(px(18.f), py(982.f)), ccp(px(1485.f), py(982.f)), ps(2.5f), blueLine);
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

inline CCNode* createTopDiamondButtonNode() {
    float w = ps(64.f), h = ps(68.f);
    auto node = CCNode::create();
    node->setContentSize({w, h});

    if (auto spr = createFitSprite("hd-diamond.png"_spr, w, h, ccp(w * 0.5f, h * 0.5f))) {
        node->addChild(spr, 2);
        return node;
    }

    auto dn = CCDrawNode::create();
    CCPoint outer[4] = {
        ccp(w * 0.5f, h * 0.95f),
        ccp(w * 0.08f, h * 0.5f),
        ccp(w * 0.5f, h * 0.05f),
        ccp(w * 0.92f, h * 0.5f)
    };
    dn->drawPolygon(
        outer, 4,
        rgba4F(ccc3(0x0c, 0x3b, 0x8e), 0.96f),
        ps(3.0f),
        rgba4F(ccc3(0x52, 0xc8, 0xff), 1.f)
    );
    CCPoint topFacet[3] = {
        ccp(w * 0.5f, h * 0.88f),
        ccp(w * 0.16f, h * 0.5f),
        ccp(w * 0.84f, h * 0.5f)
    };
    dn->drawPolygon(topFacet, 3, rgba4F(ccc3(0x38, 0xa8, 0xf8), 0.85f), 0.f, ccc4f(0, 0, 0, 0));
    node->addChild(dn);
    return node;
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
        dn, cx - rw * 0.64f, cy - rh * 0.28f, rw * 0.16f, rh * 0.56f, ps(6.f),
        rgba4F(ccc3(0x00, 0xd8, 0xff), 1.f), ps(2.5f), rgba4F(ccc3(0x02, 0x10, 0x30), 1.f)
    );
    drawRoundedRect(
        dn, cx + rw * 0.48f, cy - rh * 0.28f, rw * 0.16f, rh * 0.56f, ps(6.f),
        rgba4F(ccc3(0x00, 0xa8, 0xf8), 1.f), ps(2.5f), rgba4F(ccc3(0x02, 0x10, 0x30), 1.f)
    );
    drawRoundedRect(
        dn, cx - rw * 0.24f, cy + rh * 0.46f, rw * 0.48f, rh * 0.16f, ps(5.f),
        rgba4F(ccc3(0x0c, 0x5c, 0xb8), 1.f), ps(2.5f), rgba4F(ccc3(0x02, 0x10, 0x30), 1.f)
    );
    drawRoundedRect(
        dn, cx - rw * 0.5f, cy - rh * 0.5f, rw, rh, ps(18.f),
        rgba4F(ccc3(0x28, 0xd8, 0xff), 1.f), ps(3.5f), rgba4F(ccc3(0x02, 0x12, 0x38), 1.f)
    );
    drawRoundedRect(
        dn, cx - rw * 0.42f, cy - rh * 0.40f, rw * 0.84f, rh * 0.80f, ps(13.f),
        rgba4F(ccc3(0x02, 0x0c, 0x2c), 1.f), ps(2.f), rgba4F(ccc3(0x0a, 0x48, 0x9c), 1.f)
    );

    ccColor4F eyeCol = rgba4F(ccc3(0x18, 0xf0, 0xff), 1.f);
    float eyeR = rw * 0.12f;
    float eyeY = cy - rh * 0.04f;
    for (int side = -1; side <= 1; side += 2) {
        float ex = cx + side * (rw * 0.18f);
        for (int s = 0; s < 8; ++s) {
            float a1 = 0.35f + (2.44f * s) / 8.f;
            float a2 = 0.35f + (2.44f * (s + 1)) / 8.f;
            dn->drawSegment(
                ccp(ex + std::cos(a1) * eyeR, eyeY + std::sin(a1) * eyeR),
                ccp(ex + std::cos(a2) * eyeR, eyeY + std::sin(a2) * eyeR),
                ps(isHeaderMascot ? 4.5f : 2.5f),
                eyeCol
            );
        }
    }

    if (isHeaderMascot) {
        drawSparkle4(dn, ccp(w * 0.86f, h * 0.66f), ps(22.f), rgba4F(ccc3(0x98, 0xf8, 0xff), 1.f));
    }
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
            CCPoint tail[3] = {
                ccp(c.x - r * 0.55f, c.y - r * 0.50f),
                ccp(c.x - r * 0.80f, c.y - r * 0.95f),
                ccp(c.x - r * 0.15f, c.y - r * 0.50f)
            };
            dn->drawPolygon(tail, 3, col, 0.f, col);
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

inline CCNode* createSidebarButtonNode(int idx, char const* title, bool active) {
    float w = ps(258.f), h = ps(80.f);
    auto node = CCNode::create();
    node->setContentSize({w, h});

    ccColor3B themeAcc = ThemeConfig::getMenuAccent();
    ccColor3B actFill  = lerpColor(COL_BG_SIDE_ACT, themeAcc, 0.45f);
    ccColor3B actTop   = lerpColor(actFill, COL_TEXT_WHITE, 0.32f);
    ccColor3B idleFill = lerpColor(COL_BG_SIDE_IDLE, themeAcc, 0.20f);

    auto dn = CCDrawNode::create();
    if (active) {
        drawRoundedRect(
            dn, 0.f, 0.f, w, h, ps(12.f),
            rgba4F(actFill, 1.f),
            ps(3.2f),
            rgba4F(lerpColor(actFill, COL_TEXT_WHITE, 0.65f), 1.f)
        );
        drawRoundedRect(
            dn, ps(4.f), h * 0.52f, w - ps(8.f), h * 0.42f, ps(8.f),
            rgba4F(actTop, 0.45f), 0.f, ccc4f(0, 0, 0, 0)
        );
    } else {
        drawRoundedRect(
            dn, 0.f, 0.f, w, h, ps(12.f),
            rgba4F(idleFill, 0.95f),
            ps(2.2f),
            rgba4F(lerpColor(idleFill, themeAcc, 0.55f), 0.95f)
        );
    }

    static char const* const SB_ICONS[5] = {
        "sb-home.png"_spr, "sb-modes.png"_spr, "sb-chat.png"_spr,
        "sb-stats.png"_spr, "sb-settings.png"_spr
    };
    int sbIdx = (idx >= 0 && idx < 5) ? idx : 0;
    if (auto ico = createFitSprite(SB_ICONS[sbIdx], ps(42.f), ps(42.f), ccp(ps(52.f), h * 0.5f))) {
        ico->setColor(active ? COL_TEXT_WHITE : lerpColor(ccc3(0x67, 0x98, 0xf0), themeAcc, 0.35f));
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
    float boxW = ps(104.f), boxH = ps(84.f);
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
    float r = ps(30.f);

    switch (modeIdx) {
        case 0: {
            drawRoundedRect(dn, c.x - r * 0.85f, c.y - r * 0.65f, r * 1.1f, r * 1.1f, ps(4.f), bg, ps(3.f), col);
            drawRoundedRect(dn, c.x - r * 0.45f, c.y - r * 0.25f, r * 1.1f, r * 1.1f, ps(4.f), bg, ps(3.f), col);
            dn->drawSegment(ccp(c.x + r * 0.8f, c.y - r * 0.55f), ccp(c.x + r * 0.8f, c.y - r * 0.15f), ps(2.5f), col);
            dn->drawSegment(ccp(c.x + r * 0.6f, c.y - r * 0.35f), ccp(c.x + r * 1.0f, c.y - r * 0.35f), ps(2.5f), col);
            break;
        }
        case 1: {
            drawRoundedRect(dn, c.x - r, c.y - r * 0.75f, r * 2.f, r * 1.5f, ps(5.f), bg, ps(3.5f), col);
            dn->drawSegment(ccp(c.x + r * 0.15f, c.y - r * 0.75f), ccp(c.x + r * 0.15f, c.y + r * 0.75f), ps(2.8f), col);
            dn->drawSegment(ccp(c.x - r, c.y), ccp(c.x + r * 0.15f, c.y), ps(2.8f), col);
            break;
        }
        case 2: {
            drawGearShape(dn, ccp(c.x + r * 0.12f, c.y + r * 0.05f), r * 0.85f, col, bg);
            drawSparkle4(dn, ccp(c.x - r * 0.65f, c.y - r * 0.45f), r * 0.42f, col);
            break;
        }
        case 3: {
            drawRoundedRect(dn, c.x - r * 1.15f, c.y - r * 0.68f, r * 1.35f, r * 1.35f, ps(4.f), bg, ps(3.2f), col);
            drawSparkle4(dn, ccp(c.x + r * 0.75f, c.y), r * 0.72f, col);
            break;
        }
        case 4: {
            float s = r * 0.88f;
            CCPoint topP = ccp(c.x, c.y + s);
            CCPoint botP = ccp(c.x, c.y - s);
            CCPoint lTop = ccp(c.x - s * 0.86f, c.y + s * 0.5f);
            CCPoint rTop = ccp(c.x + s * 0.86f, c.y + s * 0.5f);
            CCPoint lBot = ccp(c.x - s * 0.86f, c.y - s * 0.5f);
            CCPoint rBot = ccp(c.x + s * 0.86f, c.y - s * 0.5f);
            CCPoint hex[6] = {topP, rTop, rBot, botP, lBot, lTop};
            dn->drawPolygon(hex, 6, bg, ps(3.2f), col);
            dn->drawSegment(c, topP, ps(2.2f), col);
            dn->drawSegment(c, lBot, ps(2.2f), col);
            dn->drawSegment(c, rBot, ps(2.2f), col);
            break;
        }
        case 5: {
            drawCircleShape(dn, c, r * 0.78f, bg, ps(3.5f), col, 20);
            dn->drawDot(c, ps(3.f), col);
            break;
        }
        case 6: {
            CCPoint arr[3] = {
                ccp(c.x - r * 0.2f, c.y + r * 0.75f),
                ccp(c.x + r * 0.75f, c.y),
                ccp(c.x - r * 0.2f, c.y - r * 0.75f)
            };
            dn->drawPolygon(arr, 3, bg, ps(3.2f), col);
            break;
        }
        case 7: {
            drawRoundedRect(dn, c.x - r * 0.95f, c.y - r * 0.65f, r * 1.9f, r * 1.3f, ps(5.f), bg, ps(3.2f), col);
            CCPoint play[3] = {
                ccp(c.x - r * 0.22f, c.y + r * 0.35f),
                ccp(c.x + r * 0.38f, c.y),
                ccp(c.x - r * 0.22f, c.y - r * 0.35f)
            };
            dn->drawPolygon(play, 3, col, 0.f, col);
            break;
        }
        default: {
            drawCircleShape(dn, ccp(c.x, c.y + r * 0.12f), r * 0.58f, bg, ps(3.2f), col, 18);
            drawRoundedRect(dn, c.x - r * 0.28f, c.y - r * 0.72f, r * 0.56f, r * 0.26f, ps(2.f), col, 0.f, col);
            break;
        }
    }
    holder->addChild(dn);
    return holder;
}

inline void drawBottomBarIcon(CCDrawNode* dn, int idx, CCPoint c) {
    ccColor4F col = rgba4F(ccc3(0x52, 0xa8, 0xf8), 1.f);
    ccColor4F dark = rgba4F(COL_BG_BOTTOM, 1.f);
    float r = ps(15.f);

    switch (idx) {
        case 0: {
            CCPoint pts[10];
            const float pi = 3.14159265f;
            for (int i = 0; i < 10; ++i) {
                float rad = (i % 2 == 0) ? r * 1.05f : r * 0.45f;
                float a = pi * 0.5f + i * (pi / 5.f);
                pts[i] = ccp(c.x + std::cos(a) * rad, c.y + std::sin(a) * rad);
            }
            dn->drawPolygon(pts, 10, col, 0.f, col);
            break;
        }
        case 1: {
            drawRoundedRect(dn, c.x - r * 1.1f, c.y - r * 0.7f, r * 1.02f, r * 1.4f, ps(2.f), col, 0.f, col);
            drawRoundedRect(dn, c.x + r * 0.08f, c.y - r * 0.7f, r * 1.02f, r * 1.4f, ps(2.f), col, 0.f, col);
            dn->drawSegment(ccp(c.x - r * 0.85f, c.y + r * 0.25f), ccp(c.x - r * 0.25f, c.y + r * 0.25f), ps(1.5f), dark);
            dn->drawSegment(ccp(c.x - r * 0.85f, c.y - r * 0.15f), ccp(c.x - r * 0.25f, c.y - r * 0.15f), ps(1.5f), dark);
            dn->drawSegment(ccp(c.x + r * 0.25f, c.y + r * 0.25f), ccp(c.x + r * 0.85f, c.y + r * 0.25f), ps(1.5f), dark);
            dn->drawSegment(ccp(c.x + r * 0.25f, c.y - r * 0.15f), ccp(c.x + r * 0.85f, c.y - r * 0.15f), ps(1.5f), dark);
            break;
        }
        case 2: {
            drawRoundedRect(dn, c.x - r * 1.05f, c.y - r * 0.70f, r * 2.1f, r * 1.25f, ps(3.f), col, 0.f, col);
            drawRoundedRect(dn, c.x - r * 1.05f, c.y + r * 0.35f, r * 0.9f, r * 0.42f, ps(2.f), col, 0.f, col);
            break;
        }
        case 3: {
            drawRoundedRect(dn, c.x - r * 1.05f, c.y - r * 0.55f, r * 2.1f, r * 0.85f, ps(5.f), col, 0.f, col);
            drawCircleShape(dn, ccp(c.x - r * 0.35f, c.y + r * 0.15f), r * 0.58f, col, 0.f, col, 14);
            drawCircleShape(dn, ccp(c.x + r * 0.30f, c.y + r * 0.05f), r * 0.48f, col, 0.f, col, 14);
            dn->drawSegment(ccp(c.x, c.y - r * 0.30f), ccp(c.x, c.y + r * 0.30f), ps(2.2f), dark);
            dn->drawSegment(ccp(c.x - r * 0.30f, c.y), ccp(c.x + r * 0.30f, c.y), ps(2.2f), dark);
            break;
        }
        default: {
            drawCircleShape(dn, c, r * 0.88f, ccc4f(0, 0, 0, 0), ps(2.8f), col, 18);
            CCPoint tip[3] = {
                ccp(c.x + r * 0.25f, c.y + r * 1.15f),
                ccp(c.x + r * 0.95f, c.y + r * 0.75f),
                ccp(c.x + r * 0.25f, c.y + r * 0.40f)
            };
            dn->drawPolygon(tip, 3, col, 0.f, col);
            break;
        }
    }
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

// Crea un boton estilizado nativo acorde al menu (reemplaza los botones grandes por defecto de GD)
inline CCNode* makeCustomMenuBtn(
    char const* labelText, float w, float h,
    ccColor3B fillCol, ccColor3B borderCol, float fontScale = 0.30f
) {
    auto node = CCNode::create();
    node->setContentSize({w, h});

    auto dn = CCDrawNode::create();
    drawRoundedRectWithSoftGradient(
        dn, 0.f, 0.f, w, h, 5.f,
        fillCol, borderCol, 0.96f,
        1.5f, rgba4F(borderCol, 1.f)
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
