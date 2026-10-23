#pragma once
// ===========================================================================
// DecoMind AI - Motor de Renderizado Visual Nativo (Cocos2d-x / Geode)
//
// Reproduce con fidelidad 1:1 la interfaz de referencia (1536x1024 -> 480x320)
// utilizando nodos nativos de Cocos2d-x (CCDrawNode, CCLayerColor, CCSprite,
// CCLabelBMFont) y combinando sprites empaquetados (_spr) con dibujo vectorial
// procedural de respaldo para que todos los iconos, bordes sci-fi, mascota
// robot y controles luzcan nitidos en cualquier resolucion.
// ===========================================================================
#include "Modes.hpp"
#include <cmath>
#include <vector>

namespace decomind {

// Dimensiones del lienzo nativo del popup (proporcion 3:2 identica a 1536x1024)
static constexpr float PW = 480.f;
static constexpr float PH = 320.f;
static constexpr float REF_W = 1536.f;
static constexpr float REF_H = 1024.f;
static constexpr float SCALE_K = PW / REF_W; // 0.3125f exacto en X e Y

// Conversores desde coordenadas de pixel de la imagen de referencia (1536x1024)
// al sistema de coordenadas de Cocos2d-x (origen abajo-izquierda, 480x320).
inline constexpr float px(float x_ref) {
    return x_ref * SCALE_K;
}
inline constexpr float py(float y_top_ref) {
    return (REF_H - y_top_ref) * SCALE_K;
}
inline constexpr float ps(float len_ref) {
    return len_ref * SCALE_K;
}

// Paleta de colores extraida directamente de la imagen de referencia
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

namespace ui {

// Genera los vertices de un rectangulo redondeado en coordenadas locales o de padre
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
        {x + w - r, y + h - r, 0.f},          // top-right
        {x + r,     y + h - r, pi * 0.5f},    // top-left
        {x + r,     y + r,     pi},           // bottom-left
        {x + w - r, y + r,     pi * 1.5f}     // bottom-right
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

// Dibuja una caja redondeada usando coordenadas de la imagen de referencia (1536x1024)
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

    // Sombra/resplandor exterior sutil cuando el borde es visible
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

// Dibuja una estrella de 4 puntas (sparkle) caracteristica de DecoMind AI
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

// Dibuja un circulo relleno + borde
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

// Dibuja el marco exterior sci-fi completo, lineas decorativas y picos de GD abajo a la izquierda
inline void drawSciFiMainFrame(CCNode* parent) {
    auto dn = CCDrawNode::create();

    // 1. Fondo general azul marino profundo
    drawRoundedRect(
        dn, 0.f, 0.f, PW, PH, ps(12.f),
        rgba4F(COL_BG_OUTER, 0.98f),
        ps(3.f), rgba4F(COL_FRAME_BLUE, 0.9f)
    );

    // Detalles arquitectonicos tenues en el fondo superior
    ccColor4F bgGrid = rgba4F(ccc3(0x06, 0x1c, 0x58), 0.45f);
    dn->drawSegment(ccp(px(260.f), py(42.f)), ccp(px(460.f), py(42.f)), ps(2.f), bgGrid);
    dn->drawSegment(ccp(px(460.f), py(42.f)), ccp(px(490.f), py(88.f)), ps(2.f), bgGrid);
    dn->drawSegment(ccp(px(490.f), py(88.f)), ccp(px(690.f), py(88.f)), ps(2.f), bgGrid);

    // 2. Contenedor principal con esquinas biseladas sci-fi (x=14..1522, y_top=120..882)
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
        rgba4F(COL_BG_FRAME, 0.96f),
        ps(3.5f),
        rgba4F(COL_FRAME_BLUE, 1.f)
    );

    // Linea superior brillante de acento cian en el marco principal
    dn->drawSegment(ccp(xL + ch, yT), ccp(xR - ch, yT), ps(2.2f), rgba4F(COL_FRAME_CYAN, 0.85f));
    dn->drawSegment(ccp(xL + ch, yB), ccp(xR - ch, yB), ps(2.0f), rgba4F(COL_FRAME_BLUE, 0.95f));

    // 3. Picos decorativos de Geometry Dash en la esquina inferior izquierda (x=38..252, y_top=735..876)
    ccColor4F cyanGlow = rgba4F(COL_FRAME_CYAN, 0.95f);
    ccColor4F blueLine = rgba4F(COL_FRAME_BLUE, 0.95f);
    ccColor4F spikeFill = rgba4F(ccc3(0x03, 0x22, 0x62), 0.95f);
    ccColor4F spikeInner = rgba4F(ccc3(0x0c, 0x52, 0xb8), 0.75f);

    // Linea vertical izquierda y pedestal horizontal
    dn->drawSegment(ccp(px(40.f), py(736.f)), ccp(px(40.f), py(874.f)), ps(3.2f), blueLine);
    dn->drawSegment(ccp(px(40.f), py(874.f)), ccp(px(252.f), py(874.f)), ps(4.0f), cyanGlow);

    // Pico grande izquierdo: base x=68..154, y_top=872, cima=(111, 786)
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

    // Pico mediano derecho: base x=146..206, y_top=872, cima=(176, 816)
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

    // Lineas escalonadas sci-fi inferiores (alrededor de la barra inferior)
    dn->drawSegment(ccp(px(18.f), py(892.f)), ccp(px(962.f), py(892.f)), ps(2.5f), blueLine);
    dn->drawSegment(ccp(px(962.f), py(892.f)), ccp(px(995.f), py(982.f)), ps(2.5f), blueLine);
    dn->drawSegment(ccp(px(18.f), py(982.f)), ccp(px(1485.f), py(982.f)), ps(2.5f), blueLine);

    parent->addChild(dn, 1);
}

// Crea el icono del boton verde de volver/cerrar (arriba a la izquierda)
inline CCNode* createBackArrowIcon() {
    float w = ps(86.f), h = ps(88.f);
    auto node = CCNode::create();
    node->setContentSize({w, h});

    auto dn = CCDrawNode::create();
    // Triangulo exterior con borde neon menta/cian y relleno verde brillante
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

// Dibuja un icono de engranaje en un CCDrawNode centrado en `c` con radio `r`
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

// Crea el boton de engranaje superior (junto a la flecha verde)
inline CCNode* createTopGearButtonNode() {
    float w = ps(82.f), h = ps(78.f);
    auto node = CCNode::create();
    node->setContentSize({w, h});

    auto dn = CCDrawNode::create();
    drawRoundedRect(
        dn, 0.f, 0.f, w, h, ps(14.f),
        rgba4F(ccc3(0x04, 0x16, 0x42), 0.96f),
        ps(3.f),
        rgba4F(ccc3(0x14, 0x52, 0xb4), 0.95f)
    );
    drawGearShape(
        dn,
        ccp(w * 0.5f, h * 0.5f),
        ps(22.f),
        rgba4F(ccc3(0x4c, 0xc8, 0xf5), 1.f),
        rgba4F(ccc3(0x04, 0x16, 0x42), 1.f)
    );
    node->addChild(dn);
    return node;
}

// Crea el icono del diamante superior derecho
inline CCNode* createTopDiamondButtonNode() {
    float w = ps(64.f), h = ps(68.f);
    auto node = CCNode::create();
    node->setContentSize({w, h});

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
    // Faceta superior clara
    CCPoint topFacet[3] = {
        ccp(w * 0.5f, h * 0.88f),
        ccp(w * 0.16f, h * 0.5f),
        ccp(w * 0.84f, h * 0.5f)
    };
    dn->drawPolygon(topFacet, 3, rgba4F(ccc3(0x38, 0xa8, 0xf8), 0.85f), 0.f, ccc4f(0, 0, 0, 0));
    node->addChild(dn);
    return node;
}

// Crea la mascota robot (ya sea para el header superior o para el banner inferior)
// Usa el sprite empaquetado si esta disponible y dibuja el robot vectorial completo como respaldo.
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

    // Dibujo vectorial nativo de alta precision del robot DecoMind AI
    auto dn = CCDrawNode::create();
    float cx = w * (isHeaderMascot ? 0.45f : 0.5f);
    float cy = h * 0.48f;
    float rw = isHeaderMascot ? ps(126.f) : ps(52.f);
    float rh = isHeaderMascot ? ps(86.f) : ps(38.f);

    // Auriculares laterales
    drawRoundedRect(
        dn, cx - rw * 0.64f, cy - rh * 0.28f, rw * 0.16f, rh * 0.56f, ps(6.f),
        rgba4F(ccc3(0x00, 0xd8, 0xff), 1.f), ps(2.5f), rgba4F(ccc3(0x02, 0x10, 0x30), 1.f)
    );
    drawRoundedRect(
        dn, cx + rw * 0.48f, cy - rh * 0.28f, rw * 0.16f, rh * 0.56f, ps(6.f),
        rgba4F(ccc3(0x00, 0xa8, 0xf8), 1.f), ps(2.5f), rgba4F(ccc3(0x02, 0x10, 0x30), 1.f)
    );

    // Antena / copete superior
    drawRoundedRect(
        dn, cx - rw * 0.24f, cy + rh * 0.46f, rw * 0.48f, rh * 0.16f, ps(5.f),
        rgba4F(ccc3(0x0c, 0x5c, 0xb8), 1.f), ps(2.5f), rgba4F(ccc3(0x02, 0x10, 0x30), 1.f)
    );

    // Carcasa exterior cian brillante
    drawRoundedRect(
        dn, cx - rw * 0.5f, cy - rh * 0.5f, rw, rh, ps(18.f),
        rgba4F(ccc3(0x28, 0xd8, 0xff), 1.f), ps(3.5f), rgba4F(ccc3(0x02, 0x12, 0x38), 1.f)
    );

    // Pantalla interior azul marino oscuro
    drawRoundedRect(
        dn, cx - rw * 0.42f, cy - rh * 0.40f, rw * 0.84f, rh * 0.80f, ps(13.f),
        rgba4F(ccc3(0x02, 0x0c, 0x2c), 1.f), ps(2.f), rgba4F(ccc3(0x0a, 0x48, 0x9c), 1.f)
    );

    // Ojos sonrientes (^ ^) en cian neon
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

// Dibuja el icono vectorial de cualquiera de los 5 botones del menu lateral
// 0: Inicio (Casa), 1: Modos (Grid 2x2), 2: Chat (Burbuja), 3: Estadisticas (Barras), 4: Ajustes (Engranaje)
inline void drawSidebarIcon(CCDrawNode* dn, int idx, CCPoint c, bool active) {
    ccColor4F col = active ? rgba4F(COL_TEXT_WHITE, 1.f) : rgba4F(ccc3(0x67, 0x98, 0xf0), 1.f);
    ccColor4F hole = active ? rgba4F(COL_BG_SIDE_ACT, 1.f) : rgba4F(COL_BG_SIDE_IDLE, 1.f);
    float r = ps(20.f);

    switch (idx) {
        case 0: { // Casa (INICIO)
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
        case 1: { // Cuadricula 2x2 (MODOS)
            float sq = r * 0.72f;
            float gap = r * 0.14f;
            drawRoundedRect(dn, c.x - gap - sq, c.y + gap, sq, sq, ps(3.f), col, 0.f, col);
            drawRoundedRect(dn, c.x + gap,      c.y + gap, sq, sq, ps(3.f), col, 0.f, col);
            drawRoundedRect(dn, c.x - gap - sq, c.y - gap - sq, sq, sq, ps(3.f), col, 0.f, col);
            drawRoundedRect(dn, c.x + gap,      c.y - gap - sq, sq, sq, ps(3.f), col, 0.f, col);
            break;
        }
        case 2: { // Burbuja de chat con 3 puntos (CHAT)
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
        case 3: { // Grafico de 3 barras (ESTADISTICAS)
            float bw = r * 0.42f;
            drawRoundedRect(dn, c.x - r * 0.82f, c.y - r * 0.80f, bw, r * 1.00f, ps(2.f), col, 0.f, col);
            drawRoundedRect(dn, c.x - bw * 0.5f, c.y - r * 0.80f, bw, r * 1.60f, ps(2.f), col, 0.f, col);
            drawRoundedRect(dn, c.x + r * 0.40f, c.y - r * 0.80f, bw, r * 0.72f, ps(2.f), col, 0.f, col);
            break;
        }
        default: { // Engranaje (AJUSTES)
            drawGearShape(dn, c, r * 0.92f, col, hole);
            break;
        }
    }
}

// Crea el nodo visual de un boton de la barra lateral (activo o inactivo)
inline CCNode* createSidebarButtonNode(int idx, char const* title, bool active) {
    float w = ps(258.f), h = ps(80.f);
    auto node = CCNode::create();
    node->setContentSize({w, h});

    auto dn = CCDrawNode::create();
    if (active) {
        // Relleno azul cian brillante con borde luminoso (como INICIO en la referencia)
        drawRoundedRect(
            dn, 0.f, 0.f, w, h, ps(12.f),
            rgba4F(COL_BG_SIDE_ACT, 1.f),
            ps(3.2f),
            rgba4F(ccc3(0x88, 0xf0, 0xff), 1.f)
        );
        // Brillo superior
        drawRoundedRect(
            dn, ps(4.f), h * 0.52f, w - ps(8.f), h * 0.42f, ps(8.f),
            rgba4F(COL_BG_SIDE_ACT_T, 0.45f), 0.f, ccc4f(0, 0, 0, 0)
        );
    } else {
        drawRoundedRect(
            dn, 0.f, 0.f, w, h, ps(12.f),
            rgba4F(COL_BG_SIDE_IDLE, 0.95f),
            ps(2.2f),
            rgba4F(ccc3(0x16, 0x38, 0x7c), 0.95f)
        );
    }

    drawSidebarIcon(dn, idx, ccp(ps(52.f), h * 0.5f), active);
    node->addChild(dn, 1);

    auto lbl = CCLabelBMFont::create(title, "bigFont.fnt");
    lbl->setAnchorPoint({0.f, 0.5f});
    lbl->setPosition({ps(92.f), h * 0.5f});
    lbl->setColor(active ? COL_TEXT_WHITE : COL_TEXT_SOFT);
    lbl->limitLabelWidth(w - ps(104.f), 0.36f, 0.18f);
    node->addChild(lbl, 2);

    return node;
}

// Crea el icono de cada uno de los 9 modos (usa el sprite extraido de la referencia
// y cuenta con dibujo vectorial completo en CCDrawNode como respaldo).
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

    // Respaldo vectorial nativo fiel a cada icono de la referencia
    auto dn = CCDrawNode::create();
    CCPoint c = ccp(boxW * 0.5f, boxH * 0.5f);
    ccColor4F col = rgba4F(m.borderColor, 1.f);
    ccColor4F bg = rgba4F(m.bgColor, 1.f);
    float r = ps(30.f);

    switch (modeIdx) {
        case 0: { // ESTRUCTURAR: capas superpuestas + signo '+'
            drawRoundedRect(dn, c.x - r * 0.85f, c.y - r * 0.65f, r * 1.1f, r * 1.1f, ps(4.f), bg, ps(3.f), col);
            drawRoundedRect(dn, c.x - r * 0.45f, c.y - r * 0.25f, r * 1.1f, r * 1.1f, ps(4.f), bg, ps(3.f), col);
            dn->drawSegment(ccp(c.x + r * 0.8f, c.y - r * 0.55f), ccp(c.x + r * 0.8f, c.y - r * 0.15f), ps(2.5f), col);
            dn->drawSegment(ccp(c.x + r * 0.6f, c.y - r * 0.35f), ccp(c.x + r * 1.0f, c.y - r * 0.35f), ps(2.5f), col);
            break;
        }
        case 1: { // LAYOUT: plano dividido
            drawRoundedRect(dn, c.x - r, c.y - r * 0.75f, r * 2.f, r * 1.5f, ps(5.f), bg, ps(3.5f), col);
            dn->drawSegment(ccp(c.x + r * 0.15f, c.y - r * 0.75f), ccp(c.x + r * 0.15f, c.y + r * 0.75f), ps(2.8f), col);
            dn->drawSegment(ccp(c.x - r, c.y), ccp(c.x + r * 0.15f, c.y), ps(2.8f), col);
            break;
        }
        case 2: { // DECO: engranaje estilizado + destello
            drawGearShape(dn, ccp(c.x + r * 0.12f, c.y + r * 0.05f), r * 0.85f, col, bg);
            drawSparkle4(dn, ccp(c.x - r * 0.65f, c.y - r * 0.45f), r * 0.42f, col);
            break;
        }
        case 3: { // LAYOUT + DECO: cuadro + estrella
            drawRoundedRect(dn, c.x - r * 1.15f, c.y - r * 0.68f, r * 1.35f, r * 1.35f, ps(4.f), bg, ps(3.2f), col);
            drawSparkle4(dn, ccp(c.x + r * 0.75f, c.y), r * 0.72f, col);
            break;
        }
        case 4: { // BLOCK DESIGN: cubo isometrico
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
        case 5: { // TRIGGERS: portal ovalado con marcas
            drawCircleShape(dn, c, r * 0.78f, bg, ps(3.5f), col, 20);
            dn->drawDot(c, ps(3.f), col);
            break;
        }
        case 6: { // TRANSICIONES: doble flecha derecha
            CCPoint arr[3] = {
                ccp(c.x - r * 0.2f, c.y + r * 0.75f),
                ccp(c.x + r * 0.75f, c.y),
                ccp(c.x - r * 0.2f, c.y - r * 0.75f)
            };
            dn->drawPolygon(arr, 3, bg, ps(3.2f), col);
            break;
        }
        case 7: { // EFECTOS AVANZADOS: pantalla con play
            drawRoundedRect(dn, c.x - r * 0.95f, c.y - r * 0.65f, r * 1.9f, r * 1.3f, ps(5.f), bg, ps(3.2f), col);
            CCPoint play[3] = {
                ccp(c.x - r * 0.22f, c.y + r * 0.35f),
                ccp(c.x + r * 0.38f, c.y),
                ccp(c.x - r * 0.22f, c.y - r * 0.35f)
            };
            dn->drawPolygon(play, 3, col, 0.f, col);
            break;
        }
        default: { // IDEAS Y SUGERENCIAS: bombilla con rayos
            drawCircleShape(dn, ccp(c.x, c.y + r * 0.12f), r * 0.58f, bg, ps(3.2f), col, 18);
            drawRoundedRect(dn, c.x - r * 0.28f, c.y - r * 0.72f, r * 0.56f, r * 0.26f, ps(2.f), col, 0.f, col);
            break;
        }
    }
    holder->addChild(dn);
    return holder;
}

// Dibuja los iconos de los 5 botones de la barra inferior:
// 0: Favoritos (Estrella), 1: Tutoriales (Libro), 2: Mis Niveles (Carpeta),
// 3: Guardar (Nube +), 4: Reiniciar (Flecha circular)
inline void drawBottomBarIcon(CCDrawNode* dn, int idx, CCPoint c) {
    ccColor4F col = rgba4F(ccc3(0x52, 0xa8, 0xf8), 1.f);
    ccColor4F dark = rgba4F(COL_BG_BOTTOM, 1.f);
    float r = ps(15.f);

    switch (idx) {
        case 0: { // Estrella 5 puntas (FAVORITOS)
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
        case 1: { // Libro abierto (TUTORIALES)
            drawRoundedRect(dn, c.x - r * 1.1f, c.y - r * 0.7f, r * 1.02f, r * 1.4f, ps(2.f), col, 0.f, col);
            drawRoundedRect(dn, c.x + r * 0.08f, c.y - r * 0.7f, r * 1.02f, r * 1.4f, ps(2.f), col, 0.f, col);
            dn->drawSegment(ccp(c.x - r * 0.85f, c.y + r * 0.25f), ccp(c.x - r * 0.25f, c.y + r * 0.25f), ps(1.5f), dark);
            dn->drawSegment(ccp(c.x - r * 0.85f, c.y - r * 0.15f), ccp(c.x - r * 0.25f, c.y - r * 0.15f), ps(1.5f), dark);
            dn->drawSegment(ccp(c.x + r * 0.25f, c.y + r * 0.25f), ccp(c.x + r * 0.85f, c.y + r * 0.25f), ps(1.5f), dark);
            dn->drawSegment(ccp(c.x + r * 0.25f, c.y - r * 0.15f), ccp(c.x + r * 0.85f, c.y - r * 0.15f), ps(1.5f), dark);
            break;
        }
        case 2: { // Carpeta (MIS NIVELES)
            drawRoundedRect(dn, c.x - r * 1.05f, c.y - r * 0.70f, r * 2.1f, r * 1.25f, ps(3.f), col, 0.f, col);
            drawRoundedRect(dn, c.x - r * 1.05f, c.y + r * 0.35f, r * 0.9f, r * 0.42f, ps(2.f), col, 0.f, col);
            break;
        }
        case 3: { // Nube con signo '+' (GUARDAR)
            drawRoundedRect(dn, c.x - r * 1.05f, c.y - r * 0.55f, r * 2.1f, r * 0.85f, ps(5.f), col, 0.f, col);
            drawCircleShape(dn, ccp(c.x - r * 0.35f, c.y + r * 0.15f), r * 0.58f, col, 0.f, col, 14);
            drawCircleShape(dn, ccp(c.x + r * 0.30f, c.y + r * 0.05f), r * 0.48f, col, 0.f, col, 14);
            dn->drawSegment(ccp(c.x, c.y - r * 0.30f), ccp(c.x, c.y + r * 0.30f), ps(2.2f), dark);
            dn->drawSegment(ccp(c.x - r * 0.30f, c.y), ccp(c.x + r * 0.30f, c.y), ps(2.2f), dark);
            break;
        }
        default: { // Flecha circular (REINICIAR)
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

} // namespace ui
} // namespace decomind
