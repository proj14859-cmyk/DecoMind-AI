// ===========================================================================
// DecoMind AI v0.3.2-alpha - Menu Interactivo Nativo para Geometry Dash
//
// Parche y modificacion del diseno v0.3.2-alpha:
//   - Selector de colores circular oficial del estilo del editor de GD
//     (rueda circular de matiz + disco central S/V + R/G/B + HEX + Copy/Paste/Default/OK),
//     eliminando el titulo "PALETA DEL MENU INTERACTIVO".
//   - Pestana AJUSTES redisenada con estilo moderno, simple y bordes delgados,
//     eliminando opciones repetidas o innecesarias.
//   - Tarjetas de los 9 modos en color entero limpio (sin degradado), sin zoom
//     brusco al pasar/pulsar, con descripciones bien espaciadas y contorno RGB
//     tipo "gusano" (flujo trasladandose de A hacia B con velocidad ajustable).
//   - Cuadro CHAT / COMANDO perfectamente encuadrado en el centro (pegado a la
//     parte superior del cuadrado central) al elegir la pestana CHAT.
//   - Eliminados el rombo y la barra de progreso de la esquina superior derecha.
//   - Botones sin degradado en color entero con iluminacion media al seleccionarlos.
//   - Panel HISTORIAL derecho sin botones al lado de cada oracion y con un unico
//     boton delgado en la esquina superior derecha para abrir la ventana.
//   - Ventana de HISTORIAL sin contornos de color en cada oracion, mostrando
//     solo texto limpio separado oracion por oracion en una lista deslizante.
// ===========================================================================
#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextArea.hpp>
#include <Geode/ui/TextInput.hpp>

#include "AIClient.hpp"
#include "HistoryManager.hpp"
#include "LevelExecutor.hpp"
#include "LevelValidator.hpp"
#include "Modes.hpp"
#include "UIRenderer.hpp"

using namespace geode::prelude;
using namespace decomind;

namespace {

static const char* SIDEBAR_NAMES[5] = {
    "INICIO", "MODOS", "CHAT", "ESTADISTICAS", "AJUSTES"
};

static const char* BOTTOM_NAMES[5] = {
    "FAVORITOS", "TUTORIALES", "MIS NIVELES", "GUARDAR", "REINICIAR"
};

static void fitBMFontLabel(CCLabelBMFont* lbl, std::string const& text, float maxW, float scale) {
    if (!lbl) return;
    std::string clean = sanitizeForGD(text);
    lbl->setString(clean.c_str());
    lbl->setScale(scale);
    lbl->limitLabelWidth(maxW, scale, 0.12f);
}

// ===========================================================================
// Clase base para ventanas secundarias
// ===========================================================================
class DMModalBase : public Popup {
protected:
    CCMenu* m_ui = nullptr;
    float m_w = 0.f;
    float m_h = 0.f;

    bool initModal(float w, float h, char const* titleText, bool showHeaderLine = true) {
        if (!Popup::init(w, h)) return false;
        m_w = w;
        m_h = h;

        m_bgSprite->setVisible(false);
        m_closeBtn->setVisible(false);

        ccColor3B acc = ThemeConfig::getMenuAccent();
        ccColor3B bgOuter = lerpColor(COL_BG_OUTER, acc, 0.12f);
        ccColor3B bgInner = lerpColor(COL_BG_FRAME, acc, 0.14f);

        auto dn = CCDrawNode::create();
        ui::drawRoundedRect(
            dn, 0.f, 0.f, w, h, 8.f,
            rgba4F(bgOuter, 0.98f),
            1.8f,
            rgba4F(lerpColor(acc, COL_FRAME_CYAN, 0.45f), 0.92f)
        );
        ui::drawRoundedRect(
            dn, 3.f, 3.f, w - 6.f, h - 6.f, 6.f,
            rgba4F(bgInner, 0.98f),
            1.0f,
            rgba4F(acc, 0.85f)
        );
        if (showHeaderLine) {
            dn->drawSegment(ccp(12.f, h - 32.f), ccp(w - 12.f, h - 32.f), 0.8f, rgba4F(acc, 0.7f));
        }
        m_mainLayer->addChild(dn, 1);

        m_ui = CCMenu::create();
        m_ui->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_ui, 50);

        if (titleText && titleText[0] != '\0') {
            auto backIcon = ui::createBackArrowIcon();
            backIcon->setScale(0.65f);
            auto closeBtn = CCMenuItemExt::createSpriteExtra(
                backIcon, [this](CCMenuItemSpriteExtra*) { this->onClose(nullptr); }
            );
            closeBtn->m_scaleMultiplier = 1.04f;
            closeBtn->setPosition({22.f, h - 17.f});
            m_ui->addChild(closeBtn);

            auto title = CCLabelBMFont::create(sanitizeForGD(titleText).c_str(), "bigFont.fnt");
            title->setColor(COL_TEXT_WHITE);
            title->setPosition({w * 0.5f, h - 17.f});
            title->limitLabelWidth(w - 80.f, 0.42f, 0.2f);
            m_mainLayer->addChild(title, 10);
        }

        return true;
    }

    CCNode* makeStyledActionBtn(char const* text, float w, float h, ccColor3B fillCol, ccColor3B borderCol) {
        return ui::makeCustomMenuBtn(text, w, h, fillCol, borderCol, 0.28f);
    }
};

// ===========================================================================
// Selector de Colores Circular Estilo Editor Oficial de Geometry Dash (Imagen 2)
// Sin el titulo "PALETA DEL MENU INTERACTIVO", con rueda circular de matiz y
// circulo central de saturacion/brillo, R/G/B, HEX, vista previa superior
// izquierda, botones Copy/Paste/Default a la derecha y boton OK abajo.
// ===========================================================================
class GDColorPickerPopup : public DMModalBase {
protected:
    float m_h = 0.60f;
    float m_s = 0.85f;
    float m_v = 0.85f;
    ccColor3B m_curColor;
    ccColor3B m_defaultColor;
    std::function<void(ccColor3B)> m_onApply;

    CCDrawNode* m_wheelNode = nullptr;
    CCDrawNode* m_cursorNode = nullptr;
    CCDrawNode* m_topLeftPreview = nullptr;
    CCLabelBMFont* m_rBoxLbl = nullptr;
    CCLabelBMFont* m_gBoxLbl = nullptr;
    CCLabelBMFont* m_bBoxLbl = nullptr;
    TextInput* m_hexInput = nullptr;
    bool m_updatingHex = false;

    static constexpr float WHEEL_CX = 186.f;
    static constexpr float WHEEL_CY = 132.f;
    static constexpr float RING_OUTER_R = 72.f;
    static constexpr float RING_INNER_R = 55.f;
    static constexpr float DISC_R = 51.f;

    bool init(
        ccColor3B initialCol,
        ccColor3B defaultCol,
        std::function<void(ccColor3B)> onApply
    ) {
        // Sin titulo "PALETA DEL MENU INTERACTIVO", siguiendo el diseno limpio del selector de GD
        if (!this->initModal(360.f, 246.f, "", false)) return false;
        m_curColor = initialCol;
        m_defaultColor = defaultCol;
        m_onApply = std::move(onApply);
        rgbToHsv(m_curColor, m_h, m_s, m_v);

        // 1. Cuadro blanco/coloreado de vista previa arriba a la izquierda (igual a Imagen 2)
        m_topLeftPreview = CCDrawNode::create();
        m_mainLayer->addChild(m_topLeftPreview, 6);

        // 2. Casillas R:, G:, B: y HEX: a la izquierda (igual a Imagen 2)
        char const* rgbTitles[3] = {"R:", "G:", "B:"};
        float rgbX[3] = {28.f, 66.f, 104.f};
        auto leftBgDn = CCDrawNode::create();
        for (int i = 0; i < 3; ++i) {
            auto tl = CCLabelBMFont::create(rgbTitles[i], "bigFont.fnt");
            tl->setScale(0.28f);
            tl->setColor(ccc3(255, 230, 120));
            tl->setPosition({rgbX[i], 168.f});
            m_mainLayer->addChild(tl, 6);

            ui::drawRoundedRect(
                leftBgDn, rgbX[i] - 16.f, 138.f, 32.f, 20.f, 4.f,
                rgba4F(ccc3(0x04, 0x0e, 0x28), 0.95f), 0.8f, rgba4F(COL_DIVIDER, 0.9f)
            );

            auto vLbl = CCLabelBMFont::create("255", "bigFont.fnt");
            vLbl->setScale(0.28f);
            vLbl->setColor(COL_TEXT_WHITE);
            vLbl->setPosition({rgbX[i], 148.f});
            m_mainLayer->addChild(vLbl, 7);
            if (i == 0) m_rBoxLbl = vLbl;
            else if (i == 1) m_gBoxLbl = vLbl;
            else m_bBoxLbl = vLbl;
        }

        auto hexTitle = CCLabelBMFont::create("HEX:", "bigFont.fnt");
        hexTitle->setScale(0.28f);
        hexTitle->setColor(ccc3(255, 230, 120));
        hexTitle->setPosition({66.f, 118.f});
        m_mainLayer->addChild(hexTitle, 6);

        ui::drawRoundedRect(
            leftBgDn, 24.f, 86.f, 84.f, 22.f, 4.f,
            rgba4F(ccc3(0x04, 0x0e, 0x28), 0.95f), 0.8f, rgba4F(COL_DIVIDER, 0.9f)
        );
        m_mainLayer->addChild(leftBgDn, 4);

        m_hexInput = TextInput::create(78.f, "FFFFFF", "bigFont.fnt");
        m_hexInput->setFilter("0123456789abcdefABCDEF");
        m_hexInput->setMaxCharCount(6);
        m_hexInput->setPosition({66.f, 97.f});
        if (m_hexInput->getBGSprite()) {
            m_hexInput->getBGSprite()->setOpacity(0);
        }
        m_hexInput->setCallback([this](std::string const& str) {
            if (m_updatingHex) return;
            if (str.size() == 6) {
                unsigned int val = std::strtoul(str.c_str(), nullptr, 16);
                m_curColor = ccc3(
                    static_cast<GLubyte>((val >> 16) & 0xFF),
                    static_cast<GLubyte>((val >> 8) & 0xFF),
                    static_cast<GLubyte>(val & 0xFF)
                );
                rgbToHsv(m_curColor, m_h, m_s, m_v);
                this->redrawColorWheel();
                this->refreshIndicators(false);
            }
        });
        m_mainLayer->addChild(m_hexInput, 12);

        // 3. Rueda Circular de Colores en el centro (Anillo exterior Hue + Circulo central S/V)
        m_wheelNode = CCDrawNode::create();
        m_mainLayer->addChild(m_wheelNode, 5);

        m_cursorNode = CCDrawNode::create();
        m_mainLayer->addChild(m_cursorNode, 8);

        // Zonas interactivas sobre el anillo circular exterior (36 sectores) y el disco interior
        const float twoPi = 6.2831853f;
        float midRingR = (RING_INNER_R + RING_OUTER_R) * 0.5f;
        for (int a = 0; a < 36; ++a) {
            float ang = twoPi * (static_cast<float>(a) / 36.f);
            float hVal = static_cast<float>(a) / 36.f;
            auto hit = CCNode::create();
            hit->setContentSize({16.f, 18.f});
            auto item = CCMenuItemExt::createSpriteExtra(hit, [this, hVal](CCMenuItemSpriteExtra*) {
                m_h = hVal;
                m_curColor = hsvToRgbColor(m_h, m_s, m_v);
                this->redrawColorWheel();
                this->refreshIndicators(true);
            });
            item->m_scaleMultiplier = 1.0f;
            item->setPosition({
                WHEEL_CX + std::cos(ang) * midRingR,
                WHEEL_CY + std::sin(ang) * midRingR
            });
            m_ui->addChild(item);
        }

        // Puntos interactivos dentro del circulo central para elegir Saturacion y Brillo
        for (int rStep = 0; rStep <= 4; ++rStep) {
            int nAngles = (rStep == 0) ? 1 : (rStep * 6);
            float radFrac = static_cast<float>(rStep) / 4.f;
            float rad = radFrac * (DISC_R - 5.f);
            for (int k = 0; k < nAngles; ++k) {
                float ang = twoPi * (static_cast<float>(k) / static_cast<float>(nAngles));
                float dx = (rStep == 0) ? 0.f : (std::cos(ang) * radFrac);
                float dy = (rStep == 0) ? 0.f : (std::sin(ang) * radFrac);
                // Mapeo suave del disco interior a S (horizontal) y V (vertical)
                float sVal = std::clamp((dx + 1.f) * 0.5f, 0.05f, 1.f);
                float vVal = std::clamp((dy + 1.f) * 0.45f + 0.15f, 0.12f, 1.f);

                auto hit = CCNode::create();
                hit->setContentSize({18.f, 18.f});
                auto item = CCMenuItemExt::createSpriteExtra(hit, [this, sVal, vVal](CCMenuItemSpriteExtra*) {
                    m_s = sVal;
                    m_v = vVal;
                    m_curColor = hsvToRgbColor(m_h, m_s, m_v);
                    this->refreshIndicators(true);
                });
                item->m_scaleMultiplier = 1.0f;
                item->setPosition({
                    WHEEL_CX + std::cos(ang) * rad,
                    WHEEL_CY + std::sin(ang) * rad
                });
                m_ui->addChild(item);
            }
        }

        // 4. Botones derechos: COPY, PASTE, DEFAULT (igual a Imagen 2)
        auto copySpr = this->makeStyledActionBtn("COPY", 62.f, 24.f, ccc3(0x16, 0x28, 0x4c), COL_TEXT_SOFT);
        auto copyBtn = CCMenuItemExt::createSpriteExtra(copySpr, [this](CCMenuItemSpriteExtra*) {
            ThemeConfig::setClipboardColor(m_curColor);
        });
        copyBtn->m_scaleMultiplier = 1.03f;
        copyBtn->setPosition({316.f, 208.f});
        m_ui->addChild(copyBtn);

        auto pasteSpr = this->makeStyledActionBtn("PASTE", 62.f, 24.f, ccc3(0x16, 0x28, 0x4c), COL_TEXT_SOFT);
        auto pasteBtn = CCMenuItemExt::createSpriteExtra(pasteSpr, [this](CCMenuItemSpriteExtra*) {
            m_curColor = ThemeConfig::getClipboardColor();
            rgbToHsv(m_curColor, m_h, m_s, m_v);
            this->redrawColorWheel();
            this->refreshIndicators(true);
        });
        pasteBtn->m_scaleMultiplier = 1.03f;
        pasteBtn->setPosition({316.f, 174.f});
        m_ui->addChild(pasteBtn);

        auto defSpr = this->makeStyledActionBtn("DEFAULT", 62.f, 24.f, ccc3(0x16, 0x28, 0x4c), COL_TEXT_SOFT);
        auto defBtn = CCMenuItemExt::createSpriteExtra(defSpr, [this](CCMenuItemSpriteExtra*) {
            m_curColor = m_defaultColor;
            rgbToHsv(m_curColor, m_h, m_s, m_v);
            this->redrawColorWheel();
            this->refreshIndicators(true);
        });
        defBtn->m_scaleMultiplier = 1.03f;
        defBtn->setPosition({316.f, 140.f});
        m_ui->addChild(defBtn);

        // 5. Boton inferior central: OK
        auto okSpr = this->makeStyledActionBtn("OK", 68.f, 26.f, ccc3(0x10, 0x5c, 0x38), COL_NEON_GREEN);
        auto okBtn = CCMenuItemExt::createSpriteExtra(okSpr, [this](CCMenuItemSpriteExtra*) {
            if (m_onApply) m_onApply(m_curColor);
            this->onClose(nullptr);
        });
        okBtn->m_scaleMultiplier = 1.03f;
        okBtn->setPosition({186.f, 24.f});
        m_ui->addChild(okBtn);

        this->redrawColorWheel();
        this->refreshIndicators(true);
        return true;
    }

    void redrawColorWheel() {
        if (!m_wheelNode) return;
        m_wheelNode->clear();

        const int segs = 60;
        const float twoPi = 6.2831853f;
        CCPoint center = ccp(WHEEL_CX, WHEEL_CY);

        // Borde blanco exterior del anillo (como en el selector oficial de GD)
        ui::drawCircleShape(
            m_wheelNode, center, RING_OUTER_R + 3.5f,
            rgba4F(COL_TEXT_WHITE, 1.f), 0.f, ccc4f(0, 0, 0, 0), 48
        );

        // Anillo arcoiris de 360 grados
        for (int i = 0; i < segs; ++i) {
            float a1 = twoPi * (static_cast<float>(i) / static_cast<float>(segs));
            float a2 = twoPi * (static_cast<float>(i + 1) / static_cast<float>(segs));
            ccColor3B c = hsvToRgbColor(static_cast<float>(i) / static_cast<float>(segs), 1.f, 1.f);
            CCPoint quad[4] = {
                ccp(center.x + std::cos(a1) * RING_INNER_R, center.y + std::sin(a1) * RING_INNER_R),
                ccp(center.x + std::cos(a1) * RING_OUTER_R, center.y + std::sin(a1) * RING_OUTER_R),
                ccp(center.x + std::cos(a2) * RING_OUTER_R, center.y + std::sin(a2) * RING_OUTER_R),
                ccp(center.x + std::cos(a2) * RING_INNER_R, center.y + std::sin(a2) * RING_INNER_R)
            };
            m_wheelNode->drawPolygon(quad, 4, rgba4F(c, 1.f), 0.f, ccc4f(0, 0, 0, 0));
        }

        // Separador oscuro entre el anillo y el disco interior
        ui::drawCircleShape(
            m_wheelNode, center, RING_INNER_R,
            rgba4F(ccc3(0x06, 0x0c, 0x20), 1.f), 0.f, ccc4f(0, 0, 0, 0), 40
        );

        // Disco central circular de Saturacion y Brillo con el matiz (m_h) actual
        const int rings = 7;
        const int slices = 28;
        for (int r = rings - 1; r >= 0; --r) {
            float r1 = DISC_R * (static_cast<float>(r) / static_cast<float>(rings));
            float r2 = DISC_R * (static_cast<float>(r + 1) / static_cast<float>(rings));
            float midFrac = (static_cast<float>(r) + 0.5f) / static_cast<float>(rings);
            for (int s = 0; s < slices; ++s) {
                float a1 = twoPi * (static_cast<float>(s) / static_cast<float>(slices));
                float a2 = twoPi * (static_cast<float>(s + 1) / static_cast<float>(slices));
                float aMid = 0.5f * (a1 + a2);
                float dx = std::cos(aMid) * midFrac;
                float dy = std::sin(aMid) * midFrac;
                float sat = std::clamp((dx + 1.f) * 0.5f, 0.05f, 1.f);
                float val = std::clamp((dy + 1.f) * 0.45f + 0.15f, 0.12f, 1.f);
                ccColor3B c = hsvToRgbColor(m_h, sat, val);

                if (r == 0) {
                    CCPoint tri[3] = {
                        center,
                        ccp(center.x + std::cos(a1) * r2, center.y + std::sin(a1) * r2),
                        ccp(center.x + std::cos(a2) * r2, center.y + std::sin(a2) * r2)
                    };
                    m_wheelNode->drawPolygon(tri, 3, rgba4F(c, 1.f), 0.f, ccc4f(0, 0, 0, 0));
                } else {
                    CCPoint quad[4] = {
                        ccp(center.x + std::cos(a1) * r1, center.y + std::sin(a1) * r1),
                        ccp(center.x + std::cos(a1) * r2, center.y + std::sin(a1) * r2),
                        ccp(center.x + std::cos(a2) * r2, center.y + std::sin(a2) * r2),
                        ccp(center.x + std::cos(a2) * r1, center.y + std::sin(a2) * r1)
                    };
                    m_wheelNode->drawPolygon(quad, 4, rgba4F(c, 1.f), 0.f, ccc4f(0, 0, 0, 0));
                }
            }
        }
    }

    void refreshIndicators(bool updateHexField) {
        if (m_topLeftPreview) {
            m_topLeftPreview->clear();
            ui::drawRoundedRect(
                m_topLeftPreview, 18.f, 192.f, 32.f, 36.f, 3.f,
                rgba4F(m_curColor, 1.f), 1.5f, rgba4F(COL_TEXT_WHITE, 0.95f)
            );
        }

        if (m_cursorNode) {
            m_cursorNode->clear();
            const float twoPi = 6.2831853f;
            // Indicador sobre el anillo exterior de matiz
            float hueAng = m_h * twoPi;
            float midRingR = (RING_INNER_R + RING_OUTER_R) * 0.5f;
            CCPoint huePt = ccp(
                WHEEL_CX + std::cos(hueAng) * midRingR,
                WHEEL_CY + std::sin(hueAng) * midRingR
            );
            ui::drawCircleShape(
                m_cursorNode, huePt, 6.5f,
                rgba4F(m_curColor, 1.f), 1.8f, rgba4F(COL_TEXT_WHITE, 1.f), 16
            );

            // Indicador circular dentro del disco central S/V
            float dx = (m_s * 2.f - 1.f);
            float dy = ((m_v - 0.15f) / 0.45f) - 1.f;
            float len = std::hypot(dx, dy);
            if (len > 1.f) { dx /= len; dy /= len; }
            CCPoint svPt = ccp(
                WHEEL_CX + dx * (DISC_R - 6.f),
                WHEEL_CY + dy * (DISC_R - 6.f)
            );
            ui::drawCircleShape(
                m_cursorNode, svPt, 5.5f,
                rgba4F(m_curColor, 1.f), 1.6f, rgba4F(COL_TEXT_WHITE, 1.f), 16
            );
        }

        if (m_rBoxLbl) m_rBoxLbl->setString(fmt::format("{}", static_cast<int>(m_curColor.r)).c_str());
        if (m_gBoxLbl) m_gBoxLbl->setString(fmt::format("{}", static_cast<int>(m_curColor.g)).c_str());
        if (m_bBoxLbl) m_bBoxLbl->setString(fmt::format("{}", static_cast<int>(m_curColor.b)).c_str());

        if (updateHexField && m_hexInput) {
            m_updatingHex = true;
            m_hexInput->setString(fmt::format(
                "{:02X}{:02X}{:02X}",
                static_cast<int>(m_curColor.r),
                static_cast<int>(m_curColor.g),
                static_cast<int>(m_curColor.b)
            ));
            m_updatingHex = false;
        }
    }

public:
    static GDColorPickerPopup* create(
        ccColor3B initialCol,
        ccColor3B defaultCol,
        std::function<void(ccColor3B)> onApply
    ) {
        auto ret = new GDColorPickerPopup();
        if (ret->init(initialCol, defaultCol, std::move(onApply))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ===========================================================================
// Ventana Secundaria 1: Historial de Peticiones
// Sin contornos de color en cada oracion: solo texto visible limpio separado
// oracion por oracion en una lista deslizante hacia abajo.
// ===========================================================================
class HistoryPopup : public DMModalBase {
protected:
    std::function<void()> m_onClear;
    ScrollLayer* m_scroll = nullptr;

    bool init(std::function<void()> onClear) {
        if (!this->initModal(340.f, 240.f, "HISTORIAL DE PETICIONES")) return false;
        m_onClear = std::move(onClear);

        ccColor3B acc = ThemeConfig::getMenuAccent();
        auto boxDn = CCDrawNode::create();
        ui::drawRoundedRect(
            boxDn, 12.f, 44.f, 316.f, 156.f, 6.f,
            rgba4F(COL_BG_CENTER, 0.95f), 1.0f, rgba4F(acc, 0.65f)
        );
        m_mainLayer->addChild(boxDn, 2);

        m_scroll = ScrollLayer::create(CCSize{304.f, 146.f});
        m_scroll->ignoreAnchorPointForPosition(false);
        m_scroll->setPosition({170.f, 122.f});
        m_mainLayer->addChild(m_scroll, 5);

        auto clearSpr = this->makeStyledActionBtn("BORRAR HISTORIAL", 136.f, 22.f, ccc3(0x32, 0x10, 0x24), ccc3(0xc8, 0x48, 0x68));
        auto clearBtn = CCMenuItemExt::createSpriteExtra(clearSpr, [this](CCMenuItemSpriteExtra*) {
            if (m_onClear) m_onClear();
            this->buildList();
        });
        clearBtn->m_scaleMultiplier = 1.03f;
        clearBtn->setPosition({170.f, 24.f});
        m_ui->addChild(clearBtn);

        this->buildList();
        return true;
    }

    void buildList() {
        auto layer = m_scroll->m_contentLayer;
        layer->removeAllChildren();

        auto const& h = HistoryManager::entries();
        float viewW = 304.f;
        float viewH = 146.f;

        if (h.empty()) {
            layer->setContentSize({viewW, viewH});
            auto emptyLbl = CCLabelBMFont::create("Aun no hay peticiones registradas.", "chatFont.fnt");
            emptyLbl->setScale(0.56f);
            emptyLbl->setColor(COL_TEXT_SOFT);
            emptyLbl->setPosition({viewW * 0.5f, viewH * 0.5f});
            layer->addChild(emptyLbl);
            m_scroll->scrollToTop();
            return;
        }

        float itemH = 30.f;
        float gap = 4.f;
        float total = 8.f + static_cast<float>(h.size()) * (itemH + gap);
        total = std::max(viewH, total);
        layer->setContentSize({viewW, total});

        // Solo texto visible separado oracion por oracion (sin recuadros/contornos de color)
        int row = 0;
        for (int k = static_cast<int>(h.size()) - 1; k >= 0; --k, ++row) {
            auto const& e = h[k];
            int mi = modeIndexById(e.modeId);
            std::string modeName = (mi >= 0) ? MODES[mi].name : "General";
            std::string relTime = HistoryManager::formatRelativeTime(e.timestamp);

            auto node = CCNode::create();
            node->setContentSize({294.f, itemH});
            node->setPosition({5.f, total - 4.f - static_cast<float>(row) * (itemH + gap) - itemH});

            std::string sentence = fmt::format("[{}] {}", modeName, e.prompt);
            auto lPrompt = CCLabelBMFont::create("", "chatFont.fnt");
            lPrompt->setAnchorPoint({0.f, 0.5f});
            lPrompt->setColor(COL_TEXT_WHITE);
            lPrompt->setPosition({8.f, itemH * 0.56f});
            fitBMFontLabel(lPrompt, sentence, 212.f, 0.50f);
            node->addChild(lPrompt, 2);

            auto lTime = CCLabelBMFont::create("", "chatFont.fnt");
            lTime->setAnchorPoint({1.f, 0.5f});
            lTime->setColor(COL_TEXT_DIM);
            lTime->setPosition({288.f, itemH * 0.56f});
            fitBMFontLabel(lTime, relTime, 68.f, 0.44f);
            node->addChild(lTime, 2);

            // Linea separadora sutil entre oracion y oracion
            auto sepDn = CCDrawNode::create();
            sepDn->drawSegment(ccp(8.f, 1.f), ccp(288.f, 1.f), 0.5f, rgba4F(COL_DIVIDER, 0.55f));
            node->addChild(sepDn, 1);

            layer->addChild(node);
        }
        m_scroll->scrollToTop();
    }

public:
    static HistoryPopup* create(std::function<void()> onClear) {
        auto ret = new HistoryPopup();
        if (ret->init(std::move(onClear))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ===========================================================================
// Ventana Secundaria 2: Favoritos
// ===========================================================================
class FavoritesPopup : public DMModalBase {
protected:
    std::string m_currentModeId;
    std::string m_currentPrompt;
    std::function<void(int, std::string const&)> m_onPick;
    ScrollLayer* m_scroll = nullptr;

    bool init(
        std::string const& curModeId,
        std::string const& curPrompt,
        std::function<void(int, std::string const&)> onPick
    ) {
        if (!this->initModal(340.f, 240.f, "PETICIONES FAVORITAS")) return false;
        m_currentModeId = curModeId;
        m_currentPrompt = trimStr(curPrompt);
        m_onPick = std::move(onPick);

        auto boxDn = CCDrawNode::create();
        ui::drawRoundedRect(
            boxDn, 12.f, 44.f, 316.f, 156.f, 6.f,
            rgba4F(COL_BG_CENTER, 0.95f), 1.0f, rgba4F(ThemeConfig::getMenuAccent(), 0.7f)
        );
        m_mainLayer->addChild(boxDn, 2);

        m_scroll = ScrollLayer::create(CCSize{304.f, 146.f});
        m_scroll->ignoreAnchorPointForPosition(false);
        m_scroll->setPosition({170.f, 122.f});
        m_mainLayer->addChild(m_scroll, 5);

        auto saveSpr = this->makeStyledActionBtn(
            "Guardar comando actual en Favoritos", 216.f, 22.f, COL_BG_SIDE_IDLE, COL_FRAME_CYAN
        );
        auto saveBtn = CCMenuItemExt::createSpriteExtra(saveSpr, [this](CCMenuItemSpriteExtra*) {
            if (m_currentPrompt.empty()) {
                FLAlertLayer::create(
                    "Favoritos",
                    "Escribe primero una instruccion en el cuadro <cy>CHAT / COMANDO</c> para guardarla.",
                    "OK"
                )->show();
                return;
            }
            HistoryManager::addFavorite(m_currentModeId.empty() ? "deco" : m_currentModeId, m_currentPrompt);
            this->buildList();
        });
        saveBtn->m_scaleMultiplier = 1.03f;
        saveBtn->setPosition({170.f, 24.f});
        m_ui->addChild(saveBtn);

        this->buildList();
        return true;
    }

    void buildList() {
        auto layer = m_scroll->m_contentLayer;
        layer->removeAllChildren();

        auto const& favs = HistoryManager::favorites();
        float viewW = 304.f;
        float viewH = 146.f;

        if (favs.empty()) {
            layer->setContentSize({viewW, viewH});
            auto lbl = CCLabelBMFont::create("No tienes favoritos guardados.", "chatFont.fnt");
            lbl->setScale(0.58f);
            lbl->setColor(COL_TEXT_SOFT);
            lbl->setPosition({viewW * 0.5f, viewH * 0.5f});
            layer->addChild(lbl);
            m_scroll->scrollToTop();
            return;
        }

        float itemH = 36.f;
        float gap = 5.f;
        float total = std::max(viewH, 6.f + static_cast<float>(favs.size()) * (itemH + gap));
        layer->setContentSize({viewW, total});

        for (size_t i = 0; i < favs.size(); ++i) {
            auto const& f = favs[i];
            int mi = modeIndexById(f.modeId);
            ccColor3B accent = (mi >= 0) ? MODES[mi].borderColor : COL_FRAME_CYAN;
            std::string modeName = (mi >= 0) ? MODES[mi].name : "General";

            auto node = CCNode::create();
            node->setContentSize({256.f, itemH});
            auto dn = CCDrawNode::create();
            ui::drawRoundedRect(
                dn, 0.f, 0.f, 256.f, itemH, 5.f,
                rgba4F(COL_BG_RIGHT_CARD, 0.96f), 0.9f, rgba4F(accent, 0.75f)
            );
            node->addChild(dn, 1);

            auto l1 = CCLabelBMFont::create("", "bigFont.fnt");
            l1->setAnchorPoint({0.f, 0.5f});
            l1->setColor(accent);
            l1->setPosition({10.f, 24.f});
            fitBMFontLabel(l1, modeName, 236.f, 0.28f);
            node->addChild(l1, 2);

            auto l2 = CCLabelBMFont::create("", "chatFont.fnt");
            l2->setAnchorPoint({0.f, 0.5f});
            l2->setColor(COL_TEXT_WHITE);
            l2->setPosition({10.f, 10.f});
            fitBMFontLabel(l2, f.prompt, 236.f, 0.48f);
            node->addChild(l2, 2);

            auto menu = CCMenu::create();
            menu->setContentSize({viewW, itemH});
            menu->setPosition({0.f, total - 4.f - static_cast<float>(i) * (itemH + gap) - itemH});

            std::string promptCopy = f.prompt;
            auto pickBtn = CCMenuItemExt::createSpriteExtra(node, [this, mi, promptCopy](CCMenuItemSpriteExtra*) {
                if (m_onPick) m_onPick(mi, promptCopy);
                this->onClose(nullptr);
            });
            pickBtn->m_scaleMultiplier = 1.02f;
            pickBtn->setPosition({134.f, itemH * 0.5f});
            menu->addChild(pickBtn);

            auto delNode = this->makeStyledActionBtn("X", 26.f, itemH - 4.f, ccc3(0x38, 0x10, 0x24), ccc3(0xd8, 0x48, 0x68));
            auto delBtn = CCMenuItemExt::createSpriteExtra(delNode, [this, i](CCMenuItemSpriteExtra*) {
                HistoryManager::removeFavorite(i);
                this->buildList();
            });
            delBtn->m_scaleMultiplier = 1.03f;
            delBtn->setPosition({282.f, itemH * 0.5f});
            menu->addChild(delBtn);

            layer->addChild(menu);
        }
        m_scroll->scrollToTop();
    }

public:
    static FavoritesPopup* create(
        std::string const& curModeId,
        std::string const& curPrompt,
        std::function<void(int, std::string const&)> onPick
    ) {
        auto ret = new FavoritesPopup();
        if (ret->init(curModeId, curPrompt, std::move(onPick))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ===========================================================================
// Ventana Secundaria 3: Tutoriales y Guia Rapida
// ===========================================================================
class TutorialsPopup : public DMModalBase {
protected:
    bool init() {
        if (!this->initModal(350.f, 240.f, "TUTORIALES Y GUIA RAPIDA")) return false;

        auto boxDn = CCDrawNode::create();
        ui::drawRoundedRect(
            boxDn, 12.f, 16.f, 326.f, 184.f, 6.f,
            rgba4F(COL_BG_CENTER, 0.95f), 1.0f, rgba4F(ThemeConfig::getMenuAccent(), 0.7f)
        );
        m_mainLayer->addChild(boxDn, 2);

        auto scroll = ScrollLayer::create(CCSize{310.f, 172.f});
        scroll->ignoreAnchorPointForPosition(false);
        scroll->setPosition({175.f, 108.f});
        m_mainLayer->addChild(scroll, 5);

        std::string guideText =
            "1. COMO USAR DECOMIND AI EN EL EDITOR\n"
            "- En INICIO, desliza y selecciona uno de los 9 modos.\n"
            "- Escribe tu instruccion en CHAT / COMANDO o abre la pestana CHAT.\n"
            "- Pulsa el boton de envio (avion de papel) para procesar la orden.\n\n"
            "2. LOS 9 MODOS ESPECIALIZADOS\n"
            "- Estructurar: organiza grupos, capas y triggers.\n"
            "- Layout: crea estructuras y saltos jugables equilibrados.\n"
            "- Deco: anade decoracion respetando los bloques solidos.\n"
            "- Layout + Deco: construye jugabilidad y decoracion en dos pasadas.\n"
            "- Block Design: disena estructuras y bloques personalizados.\n"
            "- Triggers: configura triggers automaticos en grupos libres.\n"
            "- Transiciones: conecta dos zonas o cambios de estilo suavemente.\n"
            "- Efectos Avanzados: anade movimiento, pulsos y parallax.\n"
            "- Ideas y Sugerencias: analiza el nivel y ofrece consejos.\n\n"
            "3. AJUSTES Y PERSONALIZACION\n"
            "- En AJUSTES puedes cambiar entre Modo Local y Servidor HTTP, elegir el color del menu con la rueda circular de GD y regular la velocidad del flujo RGB.";

        auto holder = CCNode::create();
        auto area = SimpleTextArea::create(guideText, "chatFont.fnt", 0.50f, 294.f);
        area->setWrappingMode(WORD_WRAP);
        area->setColor(rgba4B(COL_TEXT_WHITE, 255));
        area->setAnchorPoint({0.f, 1.f});

        float h = std::max(172.f, area->getHeight() + 16.f);
        scroll->m_contentLayer->setContentSize({310.f, h});
        holder->setContentSize({310.f, h});
        area->setPosition({8.f, h - 8.f});
        holder->addChild(area);
        scroll->m_contentLayer->addChild(holder);
        scroll->scrollToTop();

        return true;
    }

public:
    static TutorialsPopup* create() {
        auto ret = new TutorialsPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ===========================================================================
// Ventana Secundaria 4: Mis Niveles y Puntos de Guardado
// ===========================================================================
class LevelsBackupPopup : public DMModalBase {
protected:
    Ref<LevelEditorLayer> m_lel;
    ScrollLayer* m_scroll = nullptr;
    CCLabelBMFont* m_infoLbl = nullptr;

    bool init(LevelEditorLayer* lel) {
        if (!this->initModal(340.f, 240.f, "MIS NIVELES Y RESPALDOS")) return false;
        m_lel = lel;

        std::string title = LevelExecutor::getLevelTitle(lel);
        int count = LevelExecutor::getCurrentObjectCount(lel);

        m_infoLbl = CCLabelBMFont::create("", "chatFont.fnt");
        m_infoLbl->setAnchorPoint({0.5f, 0.5f});
        m_infoLbl->setColor(COL_TEXT_CYAN);
        m_infoLbl->setPosition({170.f, 194.f});
        fitBMFontLabel(m_infoLbl, fmt::format("Nivel en editor: {}  |  Objetos: {}", title, count), 312.f, 0.55f);
        m_mainLayer->addChild(m_infoLbl, 10);

        auto boxDn = CCDrawNode::create();
        ui::drawRoundedRect(
            boxDn, 12.f, 44.f, 316.f, 138.f, 6.f,
            rgba4F(COL_BG_CENTER, 0.95f), 1.0f, rgba4F(ThemeConfig::getMenuAccent(), 0.7f)
        );
        m_mainLayer->addChild(boxDn, 2);

        m_scroll = ScrollLayer::create(CCSize{304.f, 128.f});
        m_scroll->ignoreAnchorPointForPosition(false);
        m_scroll->setPosition({170.f, 113.f});
        m_mainLayer->addChild(m_scroll, 5);

        auto snapSpr = this->makeStyledActionBtn("Crear punto de guardado ahora", 190.f, 22.f, COL_BG_SIDE_IDLE, COL_NEON_GREEN);
        auto snapBtn = CCMenuItemExt::createSpriteExtra(snapSpr, [this](CCMenuItemSpriteExtra*) {
            std::string msg;
            LevelExecutor::saveLevelSnapshot(m_lel, msg);
            this->buildList();
        });
        snapBtn->m_scaleMultiplier = 1.03f;
        snapBtn->setPosition({170.f, 24.f});
        m_ui->addChild(snapBtn);

        this->buildList();
        return true;
    }

    void buildList() {
        auto layer = m_scroll->m_contentLayer;
        layer->removeAllChildren();

        auto snaps = LevelExecutor::listSnapshots();
        float viewW = 304.f;
        float viewH = 128.f;

        if (snaps.empty()) {
            layer->setContentSize({viewW, viewH});
            auto lbl = CCLabelBMFont::create("Aun no hay puntos de guardado registrados.", "chatFont.fnt");
            lbl->setScale(0.55f);
            lbl->setColor(COL_TEXT_SOFT);
            lbl->setPosition({viewW * 0.5f, viewH * 0.5f});
            layer->addChild(lbl);
            m_scroll->scrollToTop();
            return;
        }

        float itemH = 34.f;
        float gap = 5.f;
        float total = std::max(viewH, 6.f + static_cast<float>(snaps.size()) * (itemH + gap));
        layer->setContentSize({viewW, total});

        for (size_t i = 0; i < snaps.size(); ++i) {
            auto const& s = snaps[i];
            auto node = CCNode::create();
            node->setContentSize({294.f, itemH});
            node->setPosition({5.f, total - 4.f - static_cast<float>(i) * (itemH + gap) - itemH});

            auto dn = CCDrawNode::create();
            ui::drawRoundedRect(
                dn, 0.f, 0.f, 294.f, itemH, 5.f,
                rgba4F(COL_BG_RIGHT_CARD, 0.96f), 0.9f, rgba4F(ThemeConfig::getMenuAccent(), 0.75f)
            );
            node->addChild(dn, 1);

            auto l1 = CCLabelBMFont::create("", "bigFont.fnt");
            l1->setAnchorPoint({0.f, 0.5f});
            l1->setColor(COL_TEXT_WHITE);
            l1->setPosition({10.f, 22.f});
            fitBMFontLabel(l1, fmt::format("{} ({} obj)", s.levelName, s.objectCount), 274.f, 0.28f);
            node->addChild(l1, 2);

            auto l2 = CCLabelBMFont::create("", "chatFont.fnt");
            l2->setAnchorPoint({0.f, 0.5f});
            l2->setColor(COL_TEXT_SOFT);
            l2->setPosition({10.f, 9.f});
            fitBMFontLabel(l2, s.dateStr, 274.f, 0.46f);
            node->addChild(l2, 2);

            layer->addChild(node);
        }
        m_scroll->scrollToTop();
    }

public:
    static LevelsBackupPopup* create(LevelEditorLayer* lel) {
        auto ret = new LevelsBackupPopup();
        if (ret->init(lel)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ===========================================================================
// Menu Principal de DecoMind AI v0.3.2-alpha
// ===========================================================================
class DecoMindPopup : public Popup {
protected:
    Ref<LevelEditorLayer> m_lel;
    AIClient m_aiClient;

    int m_selectedMode = -1;
    int m_activePage = 0; // 0=INICIO, 1=MODOS, 2=CHAT, 3=ESTADISTICAS, 4=AJUSTES
    int m_sessionRequests = 0;
    int m_sessionPlacedObjects = 0;
    std::string m_lastReplyText = "Listo. Escribe una instruccion y pulsa el avion para enviar.";

    CCMenu* m_fixedMenu = nullptr;
    CCMenu* m_sidebarMenu = nullptr;

    CCDrawNode* m_outerFrameNode = nullptr;
    CCDrawNode* m_centerBoxNode = nullptr;
    CCDrawNode* m_spikesRgbNode = nullptr;

    CCNode* m_centerPages[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};

    CCNode* m_mascotTop = nullptr;
    float m_animTime = 0.f;
    float m_wormPhase = 0.f;

    // Pagina 0 (INICIO - Cuadricula 3x3 deslizante con color entero y flujo RGB tipo gusano)
    ScrollLayer* m_inicioScroll = nullptr;
    CCDrawNode* m_cardBorderNodes[9] = {nullptr};
    CCDrawNode* m_cardSelectionGlow[9] = {nullptr};

    // Pagina 1 (MODOS)
    CCLabelBMFont* m_modePageTitle = nullptr;
    CCLabelBMFont* m_modePageSubtitle = nullptr;
    SimpleTextArea* m_modePageGuide = nullptr;
    CCLabelBMFont* m_modePageExample = nullptr;

    // Pagina 2 (CHAT: estado inferior en el cuadro central)
    CCLabelBMFont* m_chatReplyStatusLbl = nullptr;
    CCLabelBMFont* m_chatReplyBodyLbl = nullptr;

    // Pagina 3 (ESTADISTICAS)
    CCLabelBMFont* m_statsLabels[7] = {nullptr};

    // Pagina 4 (AJUSTES simplificado y moderno)
    CCLabelBMFont* m_originValueLbl = nullptr;
    TextInput* m_serverUrlInput = nullptr;
    CCDrawNode* m_themeSwatchNode = nullptr;
    CCLabelBMFont* m_rgbStateValueLbl = nullptr;
    CCLabelBMFont* m_rgbSpeedValueLbl = nullptr;
    CCLabelBMFont* m_settingsStatusLbl = nullptr;

    // Columna Derecha - Panel 1: USO DE LA IA
    CCDrawNode* m_quotaBarFill = nullptr;
    CCLabelBMFont* m_quotaNumLbl = nullptr;
    CCLabelBMFont* m_quotaMaxLbl = nullptr;
    CCLabelBMFont* m_reqDoneValLbl = nullptr;
    CCLabelBMFont* m_curLevelValLbl = nullptr;
    CCLabelBMFont* m_activeModeValLbl = nullptr;

    // Columna Derecha - Panel 2: CHAT / COMANDO (con anclaje central propio para moverse al centro)
    CCNode* m_chatPanelNode = nullptr;
    TextInput* m_promptInput = nullptr;
    CCNode* m_placeholderNode = nullptr;
    CCLabelBMFont* m_typedPreviewLines[3] = {nullptr, nullptr, nullptr};

    // Columna Derecha - Panel 3: HISTORIAL (un unico boton '>' arriba a la derecha)
    CCNode* m_histPanelNode = nullptr;
    CCLabelBMFont* m_histRowText[3] = {nullptr, nullptr, nullptr};
    CCLabelBMFont* m_histRowTime[3] = {nullptr, nullptr, nullptr};

    // Barra Inferior Derecha - Banner del Aliado Robot
    CCDrawNode* m_allyBgNode = nullptr;
    CCDrawNode* m_allyBorderNode = nullptr;
    CCNode* m_allyRobotNode = nullptr;
    CCLabelBMFont* m_allyLine1 = nullptr;
    CCLabelBMFont* m_allyLine2 = nullptr;
    std::vector<std::string> m_phrases;
    size_t m_phraseIdx = 0;
    float m_phraseTimer = 0.f;

    // -----------------------------------------------------------------------
    bool init(LevelEditorLayer* lel) {
        if (!Popup::init(PW, PH)) return false;
        m_lel = lel;
        HistoryManager::ensureLoaded();
        m_phrases = HistoryManager::loadPhrases();

        m_bgSprite->setVisible(false);
        m_closeBtn->setVisible(false);

        auto win = CCDirector::get()->getWinSize();
        float targetScale = std::min(1.f, std::min((win.width - 8.f) / PW, (win.height - 6.f) / PH));
        m_mainLayer->setScale(targetScale);

        m_outerFrameNode = CCDrawNode::create();
        m_mainLayer->addChild(m_outerFrameNode, 1);

        m_spikesRgbNode = CCDrawNode::create();
        m_mainLayer->addChild(m_spikesRgbNode, 3);

        m_centerBoxNode = CCDrawNode::create();
        m_mainLayer->addChild(m_centerBoxNode, 2);

        m_fixedMenu = CCMenu::create();
        m_fixedMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_fixedMenu, 60);

        m_sidebarMenu = CCMenu::create();
        m_sidebarMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_sidebarMenu, 55);

        this->buildTopHeader();
        this->buildSidebar();

        for (int i = 0; i < 5; ++i) {
            m_centerPages[i] = CCNode::create();
            m_mainLayer->addChild(m_centerPages[i], 10);
        }
        this->buildPageInicioGrid();
        this->buildPageModosDetail();
        this->buildPageChatCenterContext();
        this->buildPageStats();
        this->buildPageAjustes();

        this->buildRightColumn();
        this->buildBottomBar();

        this->refreshThemeVisuals();
        this->selectMode(-1);
        this->showPage(0);
        this->refreshUsagePanel();
        this->refreshHistoryMiniPanel();
        this->refreshStatsPage();
        this->updateAllyBannerText();

        this->schedule(schedule_selector(DecoMindPopup::tick));
        return true;
    }

    void refreshThemeVisuals() {
        ccColor3B acc = ThemeConfig::getMenuAccent();
        ccColor3B centerBg = lerpColor(COL_BG_CENTER, acc, 0.14f);

        ui::drawSciFiMainFrame(m_outerFrameNode, acc);

        if (m_centerBoxNode) {
            m_centerBoxNode->clear();
            ui::drawRoundedRect(
                m_centerBoxNode,
                px(308.f), py(844.f), ps(782.f), ps(681.f), ps(22.f),
                rgba4F(centerBg, 0.96f),
                ps(2.6f),
                rgba4F(acc, 0.92f)
            );
        }
        if (m_themeSwatchNode) {
            m_themeSwatchNode->clear();
            ui::drawRoundedRect(
                m_themeSwatchNode, 0.f, 0.f, ps(42.f), ps(28.f), ps(5.f),
                rgba4F(acc, 1.f), 1.0f, rgba4F(COL_TEXT_WHITE, 0.9f)
            );
        }
        this->buildSidebar();
    }

    // -----------------------------------------------------------------------
    // 1. HEADER SUPERIOR: Solo flecha verde y mascota robot (eliminados rombo y barra derecha)
    // -----------------------------------------------------------------------
    void buildTopHeader() {
        auto backNode = ui::createBackArrowIcon();
        auto backBtn = CCMenuItemExt::createSpriteExtra(
            backNode, [this](CCMenuItemSpriteExtra*) { this->onClose(nullptr); }
        );
        backBtn->m_scaleMultiplier = 1.04f;
        backBtn->setPosition({px(72.5f), py(62.f)});
        m_fixedMenu->addChild(backBtn);

        m_mascotTop = ui::createRobotMascotNode(true);
        auto mascotBtn = CCMenuItemExt::createSpriteExtra(
            m_mascotTop, [this](CCMenuItemSpriteExtra*) { this->nextAllyPhrase(); }
        );
        mascotBtn->m_scaleMultiplier = 1.03f;
        mascotBtn->setPosition({px(828.f), py(80.f)});
        m_fixedMenu->addChild(mascotBtn);
    }

    // -----------------------------------------------------------------------
    // 2. BARRA LATERAL IZQUIERDA (Color entero e iluminacion media al seleccionarlos)
    // -----------------------------------------------------------------------
    void buildSidebar() {
        if (!m_sidebarMenu) return;
        m_sidebarMenu->removeAllChildren();
        const float yCenters[5] = {183.5f, 277.5f, 368.5f, 459.5f, 550.5f};

        for (int i = 0; i < 5; ++i) {
            bool isAct = (i == m_activePage);
            auto btnNode = ui::createSidebarButtonNode(i, SIDEBAR_NAMES[i], isAct);
            auto btn = CCMenuItemExt::createSpriteExtra(
                btnNode, [this, i](CCMenuItemSpriteExtra*) { this->showPage(i); }
            );
            // Animacion discreta sin agrandamiento excesivo
            btn->m_scaleMultiplier = 1.02f;
            btn->setPosition({px(163.f), py(yCenters[i])});
            m_sidebarMenu->addChild(btn);
        }
    }

    void showPage(int p) {
        if (p < 0 || p >= 5) p = 0;
        m_activePage = p;
        for (int i = 0; i < 5; ++i) {
            if (m_centerPages[i]) {
                m_centerPages[i]->setVisible(i == p);
            }
        }
        this->buildSidebar();

        if (m_serverUrlInput) {
            m_serverUrlInput->setEnabled(p == 4);
        }
        if (p == 1) this->refreshModeDetailPage();
        if (p == 3) this->refreshStatsPage();
        if (p == 4) this->refreshAjustesLabels();

        // Encuadre exacto de CHAT / COMANDO en el centro (Imagen 5):
        // Como m_chatPanelNode tiene su propio punto de anclaje en {0.5, 0.5} y tamano (396 x 268 ref):
        // - En la pestana CHAT (p == 2): se ubica en el centro horizontal exacto del cuadrado central
        //   (x_ref = 699.f, dentro de 308..1090) y pegado ligeramente al borde superior (y_ref = 384.f).
        // - En las demas pestanas: regresa a su posicion normal en la columna derecha (1304.f, 554.f).
        if (m_chatPanelNode && m_histPanelNode) {
            if (p == 2) {
                m_chatPanelNode->setPosition({px(699.f), py(384.f)});
                m_chatPanelNode->setScale(1.42f);
                m_histPanelNode->setPosition({0.f, ps(282.f)});
            } else {
                m_chatPanelNode->setPosition({px(1304.f), py(554.f)});
                m_chatPanelNode->setScale(1.0f);
                m_histPanelNode->setPosition({0.f, 0.f});
            }
        }
    }

    // -----------------------------------------------------------------------
    // 3. PAGINA 0 (INICIO): TARJETAS EN COLOR ENTERO, DESCRIPCIONES ESPACIADAS Y GUSANO RGB
    // -----------------------------------------------------------------------
    void buildPageInicioGrid() {
        auto page = m_centerPages[0];

        float viewW = ps(762.f);
        float viewH = ps(656.f);
        m_inicioScroll = ScrollLayer::create(CCSize{viewW, viewH});
        m_inicioScroll->ignoreAnchorPointForPosition(false);
        m_inicioScroll->setPosition({px(699.f), py(504.f)});
        page->addChild(m_inicioScroll, 5);

        auto content = m_inicioScroll->m_contentLayer;
        float totalH = viewH + ps(96.f);
        content->setContentSize({viewW, totalH});

        auto gridMenu = CCMenu::create();
        gridMenu->setPosition({0.f, 0.f});
        content->addChild(gridMenu, 10);

        const float colLocalX[3] = {ps(15.f), ps(267.f), ps(519.f)};
        const float cardW = ps(230.f);
        const float cardH = ps(198.f);
        const float rowGap = ps(26.f);
        const float topMargin = ps(14.f);

        for (int i = 0; i < 9; ++i) {
            int r = i / 3;
            int c = i % 3;
            float lx = colLocalX[c];
            float ly = totalH - topMargin - cardH - r * (cardH + rowGap);
            auto const& m = MODES[i];

            auto cardNode = CCNode::create();
            cardNode->setContentSize({cardW, cardH});

            // 1. Fondo en color entero limpio y bonito (sin el degradado escalonado)
            auto bgDn = CCDrawNode::create();
            ui::drawRoundedRect(
                bgDn, 0.f, 0.f, cardW, cardH, ps(15.f),
                rgba4F(m.bgColor, 0.96f), 0.f, ccc4f(0, 0, 0, 0)
            );
            // Separador horizontal sutil entre el titulo y la descripcion para mejorar la lectura
            bgDn->drawSegment(
                ccp(cardW * 0.20f, cardH * 0.35f),
                ccp(cardW * 0.80f, cardH * 0.35f),
                ps(1.0f),
                rgba4F(m.borderColor, 0.32f)
            );
            cardNode->addChild(bgDn, 1);

            // 2. Contorno donde viaja el flujo RGB tipo "gusano" de A hacia B
            auto borderDn = CCDrawNode::create();
            cardNode->addChild(borderDn, 2);
            m_cardBorderNodes[i] = borderDn;

            // 3. Indicador complementario suave cuando el modo esta activo
            auto selGlow = CCDrawNode::create();
            ui::drawRoundedRect(
                selGlow, ps(3.5f), ps(3.5f), cardW - ps(7.f), cardH - ps(7.f), ps(12.f),
                rgba4F(m.borderColor, 0.12f),
                ps(1.4f),
                rgba4F(COL_TEXT_WHITE, 0.78f)
            );
            selGlow->setVisible(false);
            cardNode->addChild(selGlow, 3);
            m_cardSelectionGlow[i] = selGlow;

            // 4. Icono bien centrado en la parte superior
            auto iconNode = ui::createModeIconNode(i);
            iconNode->ignoreAnchorPointForPosition(false);
            iconNode->setAnchorPoint({0.5f, 0.5f});
            iconNode->setPosition({cardW * 0.5f, cardH * 0.72f});
            cardNode->addChild(iconNode, 4);

            // 5. Titulo con jerarquia clara
            auto titleLbl = CCLabelBMFont::create(m.title, "bigFont.fnt");
            titleLbl->setColor(COL_TEXT_WHITE);
            titleLbl->setPosition({cardW * 0.5f, cardH * 0.44f});
            titleLbl->limitLabelWidth(cardW - ps(20.f), 0.27f, 0.14f);
            cardNode->addChild(titleLbl, 5);

            // 6. Descripciones bien espaciadas, legibles y con margen inferior holgado
            auto desc1 = CCLabelBMFont::create(m.visibleLine1, "chatFont.fnt");
            desc1->setColor(ccc3(0xd4, 0xec, 0xff));
            desc1->setPosition({cardW * 0.5f, cardH * 0.25f});
            desc1->limitLabelWidth(cardW - ps(26.f), 0.39f, 0.16f);
            cardNode->addChild(desc1, 5);

            auto desc2 = CCLabelBMFont::create(m.visibleLine2, "chatFont.fnt");
            desc2->setColor(ccc3(0xd4, 0xec, 0xff));
            desc2->setPosition({cardW * 0.5f, cardH * 0.14f});
            desc2->limitLabelWidth(cardW - ps(26.f), 0.39f, 0.16f);
            cardNode->addChild(desc2, 5);

            auto btn = CCMenuItemExt::createSpriteExtra(
                cardNode, [this, i](CCMenuItemSpriteExtra*) {
                    this->selectMode(i);
                }
            );
            // Sin animacion exagerada al pasar/pulsar (solo micro-respuesta de 1.5%)
            btn->m_scaleMultiplier = 1.015f;
            btn->setPosition({lx + cardW * 0.5f, ly + cardH * 0.5f});
            gridMenu->addChild(btn);
        }

        auto futureBox = CCDrawNode::create();
        ui::drawRoundedRect(
            futureBox, ps(15.f), ps(12.f), viewW - ps(30.f), ps(58.f), ps(12.f),
            rgba4F(ccc3(0x03, 0x16, 0x42), 0.75f), ps(1.2f), rgba4F(COL_DIVIDER, 0.75f)
        );
        content->addChild(futureBox, 2);

        auto futureLbl = CCLabelBMFont::create(
            "Proximamente: mas modos y herramientas inteligentes de DecoMind AI",
            "chatFont.fnt"
        );
        futureLbl->setColor(COL_TEXT_SOFT);
        futureLbl->setScale(0.45f);
        futureLbl->setPosition({viewW * 0.5f, ps(41.f)});
        content->addChild(futureLbl, 3);

        m_inicioScroll->scrollToTop();
    }

    void selectMode(int idx) {
        if (idx < -1 || idx >= 9) idx = -1;
        m_selectedMode = idx;

        for (int i = 0; i < 9; ++i) {
            if (m_cardSelectionGlow[i]) {
                m_cardSelectionGlow[i]->setVisible(i == m_selectedMode);
            }
        }

        if (m_activeModeValLbl) {
            std::string mName = (m_selectedMode >= 0) ? MODES[m_selectedMode].name : "Ninguno";
            fitBMFontLabel(m_activeModeValLbl, mName, ps(140.f), 0.48f);
            m_activeModeValLbl->setColor(
                (m_selectedMode >= 0) ? MODES[m_selectedMode].borderColor : COL_TEXT_SOFT
            );
        }

        this->refreshModeDetailPage();
        this->refreshStatsPage();
    }

    // -----------------------------------------------------------------------
    // 4. PAGINA 1 (MODOS)
    // -----------------------------------------------------------------------
    void buildPageModosDetail() {
        auto page = m_centerPages[1];

        ui::addRefRoundedBox(
            page, 336.f, 192.f, 726.f, 624.f, 16.f,
            ccc3(0x02, 0x13, 0x42), 0.94f,
            COL_FRAME_BLUE, 0.80f, 1.8f, 1
        );

        auto hdr = CCLabelBMFont::create("DETALLE DEL MODO DE CONSTRUCCION", "bigFont.fnt");
        hdr->setAnchorPoint({0.f, 0.5f});
        hdr->setColor(COL_TEXT_CYAN);
        hdr->setPosition({px(364.f), py(232.f)});
        hdr->limitLabelWidth(ps(660.f), 0.34f, 0.18f);
        page->addChild(hdr, 5);

        m_modePageTitle = CCLabelBMFont::create("", "bigFont.fnt");
        m_modePageTitle->setAnchorPoint({0.f, 0.5f});
        m_modePageTitle->setPosition({px(364.f), py(284.f)});
        page->addChild(m_modePageTitle, 5);

        m_modePageSubtitle = CCLabelBMFont::create("", "chatFont.fnt");
        m_modePageSubtitle->setAnchorPoint({0.f, 0.5f});
        m_modePageSubtitle->setColor(COL_TEXT_WHITE);
        m_modePageSubtitle->setPosition({px(364.f), py(328.f)});
        page->addChild(m_modePageSubtitle, 5);

        auto guideHdr = CCLabelBMFont::create("Que hace este modo por tu nivel:", "bigFont.fnt");
        guideHdr->setAnchorPoint({0.f, 0.5f});
        guideHdr->setColor(COL_TEXT_SOFT);
        guideHdr->setPosition({px(364.f), py(382.f)});
        guideHdr->limitLabelWidth(ps(660.f), 0.28f, 0.16f);
        page->addChild(guideHdr, 5);

        m_modePageGuide = SimpleTextArea::create(" ", "chatFont.fnt", 0.54f, ps(660.f));
        m_modePageGuide->setWrappingMode(WORD_WRAP);
        m_modePageGuide->setColor(rgba4B(COL_TEXT_WHITE, 255));
        m_modePageGuide->setAnchorPoint({0.f, 1.f});
        m_modePageGuide->setPosition({px(364.f), py(410.f)});
        page->addChild(m_modePageGuide, 5);

        auto exHdr = CCLabelBMFont::create("Ejemplo de instruccion recomendada:", "bigFont.fnt");
        exHdr->setAnchorPoint({0.f, 0.5f});
        exHdr->setColor(COL_TEXT_SOFT);
        exHdr->setPosition({px(364.f), py(555.f)});
        exHdr->limitLabelWidth(ps(660.f), 0.28f, 0.16f);
        page->addChild(exHdr, 5);

        m_modePageExample = CCLabelBMFont::create("", "chatFont.fnt");
        m_modePageExample->setAnchorPoint({0.f, 0.5f});
        m_modePageExample->setColor(COL_TEXT_CYAN);
        m_modePageExample->setPosition({px(364.f), py(595.f)});
        page->addChild(m_modePageExample, 5);

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        page->addChild(menu, 10);

        auto prevSpr = ui::makeCustomMenuBtn("< Anterior", ps(156.f), ps(44.f), COL_BG_SIDE_IDLE, COL_FRAME_BLUE, 0.26f);
        auto prevBtn = CCMenuItemExt::createSpriteExtra(prevSpr, [this](CCMenuItemSpriteExtra*) {
            int next = (m_selectedMode <= 0) ? 8 : (m_selectedMode - 1);
            this->selectMode(next);
        });
        prevBtn->m_scaleMultiplier = 1.03f;
        prevBtn->setPosition({px(444.f), py(730.f)});
        menu->addChild(prevBtn);

        auto nextSpr = ui::makeCustomMenuBtn("Siguiente >", ps(156.f), ps(44.f), COL_BG_SIDE_IDLE, COL_FRAME_BLUE, 0.26f);
        auto nextBtn = CCMenuItemExt::createSpriteExtra(nextSpr, [this](CCMenuItemSpriteExtra*) {
            int next = (m_selectedMode < 0) ? 0 : ((m_selectedMode + 1) % 9);
            this->selectMode(next);
        });
        nextBtn->m_scaleMultiplier = 1.03f;
        nextBtn->setPosition({px(698.f), py(730.f)});
        menu->addChild(nextBtn);

        auto useExSpr = ui::makeCustomMenuBtn("Cargar ejemplo en Chat", ps(260.f), ps(44.f), ccc3(0x0a, 0x36, 0x52), COL_FRAME_CYAN, 0.26f);
        auto useExBtn = CCMenuItemExt::createSpriteExtra(useExSpr, [this](CCMenuItemSpriteExtra*) {
            int idx = (m_selectedMode >= 0) ? m_selectedMode : 2;
            this->selectMode(idx);
            this->setPromptText(MODES[idx].examplePrompt);
            this->showPage(2);
        });
        useExBtn->m_scaleMultiplier = 1.03f;
        useExBtn->setPosition({px(571.f), py(660.f)});
        menu->addChild(useExBtn);

        auto gridSpr = ui::makeCustomMenuBtn("Ver cuadricula", ps(180.f), ps(44.f), COL_BG_SIDE_IDLE, COL_FRAME_CYAN, 0.26f);
        auto gridBtn = CCMenuItemExt::createSpriteExtra(gridSpr, [this](CCMenuItemSpriteExtra*) {
            this->showPage(0);
        });
        gridBtn->m_scaleMultiplier = 1.03f;
        gridBtn->setPosition({px(936.f), py(730.f)});
        menu->addChild(gridBtn);
    }

    void refreshModeDetailPage() {
        if (!m_modePageTitle) return;
        int idx = (m_selectedMode >= 0 && m_selectedMode < 9) ? m_selectedMode : 0;
        auto const& m = MODES[idx];

        fitBMFontLabel(m_modePageTitle, m.title, ps(660.f), 0.48f);
        m_modePageTitle->setColor(m.borderColor);

        std::string sub = std::string(m.visibleLine1) + " " + m.visibleLine2;
        fitBMFontLabel(m_modePageSubtitle, sub, ps(660.f), 0.56f);

        if (m_modePageGuide) m_modePageGuide->setText(m.userGuide);
        if (m_modePageExample) {
            fitBMFontLabel(m_modePageExample, std::string("\"") + m.examplePrompt + "\"", ps(660.f), 0.52f);
        }
    }

    // -----------------------------------------------------------------------
    // 5. PAGINA 2 (CHAT): PANEL DE RESPUESTA DEBAJO DEL CHAT CENTRADO (Imagen 5)
    // -----------------------------------------------------------------------
    void buildPageChatCenterContext() {
        auto page = m_centerPages[2];

        ui::addRefRoundedBox(
            page, 344.f, 616.f, 710.f, 198.f, 14.f,
            ccc3(0x02, 0x12, 0x3a), 0.94f,
            COL_DIVIDER, 0.85f, 1.6f, 1
        );

        m_chatReplyStatusLbl = CCLabelBMFont::create("ESTADO DE LA IA: LISTO", "bigFont.fnt");
        m_chatReplyStatusLbl->setAnchorPoint({0.f, 0.5f});
        m_chatReplyStatusLbl->setColor(COL_NEON_GREEN);
        m_chatReplyStatusLbl->setPosition({px(368.f), py(652.f)});
        m_chatReplyStatusLbl->limitLabelWidth(ps(660.f), 0.28f, 0.16f);
        page->addChild(m_chatReplyStatusLbl, 5);

        m_chatReplyBodyLbl = CCLabelBMFont::create("", "chatFont.fnt");
        m_chatReplyBodyLbl->setAnchorPoint({0.f, 0.5f});
        m_chatReplyBodyLbl->setColor(COL_TEXT_WHITE);
        m_chatReplyBodyLbl->setPosition({px(368.f), py(718.f)});
        fitBMFontLabel(m_chatReplyBodyLbl, m_lastReplyText, ps(660.f), 0.50f);
        page->addChild(m_chatReplyBodyLbl, 5);
    }

    // -----------------------------------------------------------------------
    // 6. PAGINA 3 (ESTADISTICAS)
    // -----------------------------------------------------------------------
    void buildPageStats() {
        auto page = m_centerPages[3];

        ui::addRefRoundedBox(
            page, 336.f, 192.f, 726.f, 624.f, 16.f,
            ccc3(0x02, 0x13, 0x42), 0.94f,
            COL_FRAME_BLUE, 0.80f, 1.8f, 1
        );

        auto hdr = CCLabelBMFont::create("ESTADISTICAS DE USO Y NIVEL", "bigFont.fnt");
        hdr->setAnchorPoint({0.f, 0.5f});
        hdr->setColor(COL_TEXT_CYAN);
        hdr->setPosition({px(364.f), py(236.f)});
        hdr->limitLabelWidth(ps(660.f), 0.36f, 0.18f);
        page->addChild(hdr, 5);

        for (int i = 0; i < 7; ++i) {
            float ry = 310.f + static_cast<float>(i) * 68.f;
            ui::addRefRoundedBox(
                page, 360.f, ry - 24.f, 678.f, 52.f, 10.f,
                COL_BG_RIGHT_CARD, 0.92f, COL_DIVIDER, 0.75f, 1.2f, 2
            );
            m_statsLabels[i] = CCLabelBMFont::create("", "chatFont.fnt");
            m_statsLabels[i]->setAnchorPoint({0.f, 0.5f});
            m_statsLabels[i]->setColor(COL_TEXT_WHITE);
            m_statsLabels[i]->setPosition({px(382.f), py(ry + 2.f)});
            page->addChild(m_statsLabels[i], 5);
        }
    }

    void refreshStatsPage() {
        if (!m_statsLabels[0]) return;
        std::string origin = AIClient::getOriginMode();
        int used = Mod::get()->getSavedValue<int>("used", 0);
        int limit = Mod::get()->getSavedValue<int>("limit", 20);
        int total = Mod::get()->getSavedValue<int>("total", 0);
        std::string modeName = (m_selectedMode >= 0) ? MODES[m_selectedMode].name : "Ninguno";
        std::string lvlName = LevelExecutor::getLevelTitle(m_lel);
        int lvlObjs = LevelExecutor::getCurrentObjectCount(m_lel);

        std::string rows[7] = {
            "Origen configurado: " + std::string(origin == "local" ? "Local (modo de prueba sin red)" : "Servidor HTTP (Proveedor IA)"),
            "Modo activo seleccionado: " + modeName,
            origin == "local"
                ? fmt::format("Cuota del servidor: {} / {} (origen local no consume cuota)", used, limit)
                : fmt::format("Limite diario del servidor: {} / {} peticiones", used, limit),
            fmt::format("Peticiones realizadas en esta sesion: {}", m_sessionRequests),
            fmt::format("Peticiones totales acumuladas: {}", total),
            fmt::format("Objetos validados y colocados (sesion): {}", m_sessionPlacedObjects),
            fmt::format("Nivel actual: {} ({} objetos en el editor)", lvlName, lvlObjs)
        };

        for (int i = 0; i < 7; ++i) {
            fitBMFontLabel(m_statsLabels[i], rows[i], ps(636.f), 0.56f);
        }
    }

    // -----------------------------------------------------------------------
    // 7. PAGINA 4 (AJUSTES): REDISENO MODERNO, SIMPLE Y SIN OPCIONES REPETIDAS (Imagen 3)
    // -----------------------------------------------------------------------
    void buildPageAjustes() {
        auto page = m_centerPages[4];

        ui::addRefRoundedBox(
            page, 336.f, 190.f, 726.f, 624.f, 16.f,
            ccc3(0x02, 0x12, 0x3b), 0.95f,
            COL_DIVIDER, 0.80f, 1.5f, 1
        );

        auto hdr = CCLabelBMFont::create("AJUSTES DEL MENU", "bigFont.fnt");
        hdr->setAnchorPoint({0.f, 0.5f});
        hdr->setColor(COL_TEXT_WHITE);
        hdr->setPosition({px(368.f), py(236.f)});
        hdr->limitLabelWidth(ps(640.f), 0.34f, 0.18f);
        page->addChild(hdr, 5);

        // 4 filas limpias y modernas separadas por divisores delgados (sin cajas recargadas)
        auto linesDn = CCDrawNode::create();
        ccColor4F sepCol = rgba4F(COL_DIVIDER, 0.65f);
        linesDn->drawSegment(ccp(px(364.f), py(264.f)), ccp(px(1032.f), py(264.f)), ps(1.0f), sepCol);
        linesDn->drawSegment(ccp(px(364.f), py(376.f)), ccp(px(1032.f), py(376.f)), ps(0.8f), sepCol);
        linesDn->drawSegment(ccp(px(364.f), py(504.f)), ccp(px(1032.f), py(504.f)), ps(0.8f), sepCol);
        linesDn->drawSegment(ccp(px(364.f), py(614.f)), ccp(px(1032.f), py(614.f)), ps(0.8f), sepCol);
        linesDn->drawSegment(ccp(px(364.f), py(736.f)), ccp(px(1032.f), py(736.f)), ps(0.8f), sepCol);
        page->addChild(linesDn, 2);

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        page->addChild(menu, 10);

        ccColor3B btnBg   = ccc3(0x0a, 0x24, 0x58);
        ccColor3B btnBord = ccc3(0x28, 0x62, 0xb8);

        // --- 1. Modo de conexion (y = 320) ---
        auto lConn = CCLabelBMFont::create("Modo de conexion", "chatFont.fnt");
        lConn->setAnchorPoint({0.f, 0.5f});
        lConn->setColor(COL_TEXT_SOFT);
        lConn->setScale(0.54f);
        lConn->setPosition({px(372.f), py(300.f)});
        page->addChild(lConn, 5);

        m_originValueLbl = CCLabelBMFont::create("", "bigFont.fnt");
        m_originValueLbl->setAnchorPoint({0.f, 0.5f});
        m_originValueLbl->setPosition({px(372.f), py(338.f)});
        page->addChild(m_originValueLbl, 5);

        auto toggleSpr = ui::makeCustomMenuBtn("Cambiar", ps(154.f), ps(44.f), btnBg, btnBord, 0.26f);
        auto toggleBtn = CCMenuItemExt::createSpriteExtra(toggleSpr, [this](CCMenuItemSpriteExtra*) {
            std::string cur = AIClient::getOriginMode();
            std::string next = (cur == "local") ? "server" : "local";
            AIClient::setOriginMode(next);
            this->refreshAjustesLabels();
            this->refreshUsagePanel();
            this->refreshStatsPage();
            this->setAjustesStatus(next == "local" ? "Modo cambiado a Local" : "Modo cambiado a Servidor HTTP");
        });
        toggleBtn->m_scaleMultiplier = 1.03f;
        toggleBtn->setPosition({px(948.f), py(320.f)});
        menu->addChild(toggleBtn);

        // --- 2. URL del servidor (y = 440) ---
        auto lUrl = CCLabelBMFont::create("Direccion del servidor (HTTP)", "chatFont.fnt");
        lUrl->setAnchorPoint({0.f, 0.5f});
        lUrl->setColor(COL_TEXT_SOFT);
        lUrl->setScale(0.52f);
        lUrl->setPosition({px(372.f), py(408.f)});
        page->addChild(lUrl, 5);

        m_serverUrlInput = TextInput::create(ps(460.f), "http://127.0.0.1:8765/generate", "chatFont.fnt");
        m_serverUrlInput->setFilter("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/.:-_");
        m_serverUrlInput->setMaxCharCount(96);
        m_serverUrlInput->setString(AIClient::getServerUrl());
        m_serverUrlInput->setPosition({px(604.f), py(458.f)});
        page->addChild(m_serverUrlInput, 12);

        auto saveUrlSpr = ui::makeCustomMenuBtn("Guardar", ps(154.f), ps(44.f), btnBg, btnBord, 0.26f);
        auto saveUrlBtn = CCMenuItemExt::createSpriteExtra(saveUrlSpr, [this](CCMenuItemSpriteExtra*) {
            std::string val = trimStr(std::string(m_serverUrlInput->getString().c_str()));
            if (val.empty()) {
                this->setAjustesStatus("La direccion no puede estar vacia.");
                return;
            }
            AIClient::setServerUrl(val);
            this->setAjustesStatus("Direccion del servidor guardada.");
        });
        saveUrlBtn->m_scaleMultiplier = 1.03f;
        saveUrlBtn->setPosition({px(948.f), py(458.f)});
        menu->addChild(saveUrlBtn);

        // --- 3. Color del menu (Rueda circular oficial de GD) (y = 558) ---
        auto lCol = CCLabelBMFont::create("Color principal del menu", "chatFont.fnt");
        lCol->setAnchorPoint({0.f, 0.5f});
        lCol->setColor(COL_TEXT_WHITE);
        lCol->setScale(0.56f);
        lCol->setPosition({px(372.f), py(558.f)});
        page->addChild(lCol, 5);

        m_themeSwatchNode = CCDrawNode::create();
        m_themeSwatchNode->setPosition({px(796.f), py(572.f)});
        page->addChild(m_themeSwatchNode, 6);

        auto pickColSpr = ui::makeCustomMenuBtn("Seleccionar", ps(154.f), ps(44.f), btnBg, btnBord, 0.26f);
        auto pickColBtn = CCMenuItemExt::createSpriteExtra(pickColSpr, [this](CCMenuItemSpriteExtra*) {
            if (auto pop = GDColorPickerPopup::create(
                ThemeConfig::getMenuAccent(),
                COL_FRAME_BLUE,
                [this](ccColor3B chosen) {
                    ThemeConfig::setMenuAccent(chosen);
                    this->refreshThemeVisuals();
                    this->setAjustesStatus("Color del menu actualizado.");
                }
            )) {
                pop->show();
            }
        });
        pickColBtn->m_scaleMultiplier = 1.03f;
        pickColBtn->setPosition({px(948.f), py(558.f)});
        menu->addChild(pickColBtn);

        // --- 4. Flujo RGB en contornos y velocidad del gusano (y = 674) ---
        auto lRgb = CCLabelBMFont::create("Flujo RGB en contornos", "chatFont.fnt");
        lRgb->setAnchorPoint({0.f, 0.5f});
        lRgb->setColor(COL_TEXT_WHITE);
        lRgb->setScale(0.56f);
        lRgb->setPosition({px(372.f), py(654.f)});
        page->addChild(lRgb, 5);

        m_rgbStateValueLbl = CCLabelBMFont::create("", "bigFont.fnt");
        m_rgbStateValueLbl->setAnchorPoint({0.f, 0.5f});
        m_rgbStateValueLbl->setPosition({px(372.f), py(696.f)});
        page->addChild(m_rgbStateValueLbl, 5);

        auto rgbToggleSpr = ui::makeCustomMenuBtn("Alternar", ps(126.f), ps(42.f), btnBg, btnBord, 0.25f);
        auto rgbToggleBtn = CCMenuItemExt::createSpriteExtra(rgbToggleSpr, [this](CCMenuItemSpriteExtra*) {
            ThemeConfig::setRgbEnabled(!ThemeConfig::isRgbEnabled());
            this->refreshAjustesLabels();
            this->setAjustesStatus(
                ThemeConfig::isRgbEnabled() ? "Flujo RGB activado." : "Contornos en color fijo."
            );
        });
        rgbToggleBtn->m_scaleMultiplier = 1.03f;
        rgbToggleBtn->setPosition({px(652.f), py(676.f)});
        menu->addChild(rgbToggleBtn);

        auto spdMinusSpr = ui::makeCustomMenuBtn("-", ps(44.f), ps(42.f), btnBg, btnBord, 0.34f);
        auto spdMinusBtn = CCMenuItemExt::createSpriteExtra(spdMinusSpr, [this](CCMenuItemSpriteExtra*) {
            ThemeConfig::setRgbSpeedPct(ThemeConfig::getRgbSpeedPct() - 25);
            this->refreshAjustesLabels();
        });
        spdMinusBtn->m_scaleMultiplier = 1.03f;
        spdMinusBtn->setPosition({px(810.f), py(676.f)});
        menu->addChild(spdMinusBtn);

        m_rgbSpeedValueLbl = CCLabelBMFont::create("", "bigFont.fnt");
        m_rgbSpeedValueLbl->setPosition({px(906.f), py(676.f)});
        m_rgbSpeedValueLbl->setScale(0.30f);
        page->addChild(m_rgbSpeedValueLbl, 5);

        auto spdPlusSpr = ui::makeCustomMenuBtn("+", ps(44.f), ps(42.f), btnBg, btnBord, 0.34f);
        auto spdPlusBtn = CCMenuItemExt::createSpriteExtra(spdPlusSpr, [this](CCMenuItemSpriteExtra*) {
            ThemeConfig::setRgbSpeedPct(ThemeConfig::getRgbSpeedPct() + 25);
            this->refreshAjustesLabels();
        });
        spdPlusBtn->m_scaleMultiplier = 1.03f;
        spdPlusBtn->setPosition({px(1002.f), py(676.f)});
        menu->addChild(spdPlusBtn);

        m_settingsStatusLbl = CCLabelBMFont::create("", "chatFont.fnt");
        m_settingsStatusLbl->setAnchorPoint({0.f, 0.5f});
        m_settingsStatusLbl->setColor(COL_TEXT_SOFT);
        m_settingsStatusLbl->setPosition({px(372.f), py(774.f)});
        fitBMFontLabel(m_settingsStatusLbl, "Listo.", ps(640.f), 0.48f);
        page->addChild(m_settingsStatusLbl, 5);

        this->refreshAjustesLabels();
    }

    void refreshAjustesLabels() {
        if (m_originValueLbl) {
            std::string origin = AIClient::getOriginMode();
            bool isLocal = (origin == "local");
            fitBMFontLabel(
                m_originValueLbl,
                isLocal ? "Modo Local (sin red)" : "Servidor HTTP (IA activa)",
                ps(460.f), 0.30f
            );
            m_originValueLbl->setColor(isLocal ? ccc3(0xfb, 0xcf, 0x50) : COL_NEON_GREEN);
        }
        if (m_rgbStateValueLbl) {
            bool en = ThemeConfig::isRgbEnabled();
            fitBMFontLabel(m_rgbStateValueLbl, en ? "Flujo activo" : "Color fijo", ps(210.f), 0.28f);
            m_rgbStateValueLbl->setColor(en ? COL_NEON_GREEN : COL_TEXT_SOFT);
        }
        if (m_rgbSpeedValueLbl) {
            m_rgbSpeedValueLbl->setString(fmt::format("{}%", ThemeConfig::getRgbSpeedPct()).c_str());
        }
    }

    void setAjustesStatus(std::string const& s) {
        if (m_settingsStatusLbl) {
            fitBMFontLabel(m_settingsStatusLbl, s, ps(640.f), 0.48f);
        }
    }

    // -----------------------------------------------------------------------
    // 8. COLUMNA DERECHA: USO DE LA IA, CHAT / COMANDO (Centrable) e HISTORIAL
    // -----------------------------------------------------------------------
    void buildRightColumn() {
        // -------------------------------------------------------------------
        // PANEL 1: USO DE LA IA (x=1106..1502, y_top=142..406)
        // -------------------------------------------------------------------
        ui::addRefRoundedBox(
            m_mainLayer,
            1106.f, 142.f, 396.f, 264.f, 18.f,
            COL_BG_RIGHT_CARD, 0.96f,
            COL_FRAME_BLUE, 0.90f, 2.2f, 3
        );

        auto usoDn = CCDrawNode::create();
        ui::drawSparkle4(usoDn, ccp(px(1138.f), py(169.f)), ps(14.f), rgba4F(ccc3(0x52, 0xd8, 0xff), 1.f));
        ccColor4F divCol = rgba4F(COL_DIVIDER, 0.85f);
        usoDn->drawSegment(ccp(px(1122.f), py(192.f)), ccp(px(1486.f), py(192.f)), ps(1.5f), divCol);
        usoDn->drawSegment(ccp(px(1122.f), py(304.f)), ccp(px(1486.f), py(304.f)), ps(1.5f), divCol);
        usoDn->drawSegment(ccp(px(1122.f), py(348.f)), ccp(px(1486.f), py(348.f)), ps(1.5f), divCol);

        ccColor4F iconSoft = rgba4F(COL_TEXT_SOFT, 0.95f);
        usoDn->drawSegment(ccp(px(1130.f), py(212.f)), ccp(px(1140.f), py(212.f)), ps(1.8f), rgba4F(COL_BAR_TEAL, 1.f));
        usoDn->drawSegment(ccp(px(1135.f), py(207.f)), ccp(px(1135.f), py(217.f)), ps(1.8f), rgba4F(COL_BAR_TEAL, 1.f));

        ui::drawRoundedRect(
            usoDn,
            px(1138.f), py(255.f), ps(326.f), ps(20.f), ps(10.f),
            rgba4F(ccc3(0x09, 0x1c, 0x48), 1.f), 0.f, ccc4f(0, 0, 0, 0)
        );
        ui::drawRoundedRect(
            usoDn, px(1128.f), py(290.f), ps(20.f), ps(15.f), ps(4.f),
            iconSoft, 0.f, iconSoft
        );
        CCPoint playPts[3] = {
            ccp(px(1131.f), py(318.f)),
            ccp(px(1146.f), py(326.f)),
            ccp(px(1131.f), py(334.f))
        };
        usoDn->drawPolygon(playPts, 3, iconSoft, 0.f, iconSoft);
        ui::drawRoundedRect(
            usoDn, px(1127.f), py(380.f), ps(22.f), ps(15.f), ps(5.f),
            iconSoft, 0.f, iconSoft
        );
        usoDn->drawDot(ccp(px(1138.f), py(372.5f)), ps(3.f), rgba4F(COL_BG_RIGHT_CARD, 1.f));
        m_mainLayer->addChild(usoDn, 4);

        m_quotaBarFill = CCDrawNode::create();
        m_mainLayer->addChild(m_quotaBarFill, 5);

        auto usoTitle = CCLabelBMFont::create("USO DE LA IA", "bigFont.fnt");
        usoTitle->setAnchorPoint({0.f, 0.5f});
        usoTitle->setColor(COL_TEXT_CYAN);
        usoTitle->setPosition({px(1164.f), py(169.f)});
        usoTitle->limitLabelWidth(ps(280.f), 0.34f, 0.18f);
        m_mainLayer->addChild(usoTitle, 6);

        auto limLbl = CCLabelBMFont::create("Limite diario", "chatFont.fnt");
        limLbl->setAnchorPoint({0.f, 0.5f});
        limLbl->setColor(COL_TEXT_SOFT);
        limLbl->setScale(0.50f);
        limLbl->setPosition({px(1150.f), py(212.f)});
        m_mainLayer->addChild(limLbl, 6);

        m_quotaNumLbl = CCLabelBMFont::create("0", "bigFont.fnt");
        m_quotaNumLbl->setAnchorPoint({1.f, 0.5f});
        m_quotaNumLbl->setColor(COL_TEXT_WHITE);
        m_quotaNumLbl->setScale(0.32f);
        m_quotaNumLbl->setPosition({px(1432.f), py(212.f)});
        m_mainLayer->addChild(m_quotaNumLbl, 6);

        m_quotaMaxLbl = CCLabelBMFont::create("/ 20", "chatFont.fnt");
        m_quotaMaxLbl->setAnchorPoint({0.f, 0.5f});
        m_quotaMaxLbl->setColor(COL_TEXT_SOFT);
        m_quotaMaxLbl->setScale(0.50f);
        m_quotaMaxLbl->setPosition({px(1436.f), py(212.f)});
        m_mainLayer->addChild(m_quotaMaxLbl, 6);

        auto reqLbl = CCLabelBMFont::create("Peticiones realizadas", "chatFont.fnt");
        reqLbl->setAnchorPoint({0.f, 0.5f});
        reqLbl->setColor(COL_TEXT_SOFT);
        reqLbl->setScale(0.48f);
        reqLbl->setPosition({px(1160.f), py(282.f)});
        m_mainLayer->addChild(reqLbl, 6);

        m_reqDoneValLbl = CCLabelBMFont::create("0", "bigFont.fnt");
        m_reqDoneValLbl->setAnchorPoint({1.f, 0.5f});
        m_reqDoneValLbl->setColor(COL_TEXT_WHITE);
        m_reqDoneValLbl->setScale(0.32f);
        m_reqDoneValLbl->setPosition({px(1480.f), py(282.f)});
        m_mainLayer->addChild(m_reqDoneValLbl, 6);

        auto nivLbl = CCLabelBMFont::create("Nivel actual", "chatFont.fnt");
        nivLbl->setAnchorPoint({0.f, 0.5f});
        nivLbl->setColor(COL_TEXT_SOFT);
        nivLbl->setScale(0.48f);
        nivLbl->setPosition({px(1160.f), py(326.f)});
        m_mainLayer->addChild(nivLbl, 6);

        m_curLevelValLbl = CCLabelBMFont::create("Sin titulo", "bigFont.fnt");
        m_curLevelValLbl->setAnchorPoint({1.f, 0.5f});
        m_curLevelValLbl->setColor(COL_TEXT_WHITE);
        m_curLevelValLbl->setPosition({px(1480.f), py(326.f)});
        fitBMFontLabel(m_curLevelValLbl, LevelExecutor::getLevelTitle(m_lel), ps(155.f), 0.30f);
        m_mainLayer->addChild(m_curLevelValLbl, 6);

        auto modLbl = CCLabelBMFont::create("Modo activo", "chatFont.fnt");
        modLbl->setAnchorPoint({0.f, 0.5f});
        modLbl->setColor(COL_TEXT_SOFT);
        modLbl->setScale(0.48f);
        modLbl->setPosition({px(1160.f), py(372.f)});
        m_mainLayer->addChild(modLbl, 6);

        m_activeModeValLbl = CCLabelBMFont::create("Ninguno", "chatFont.fnt");
        m_activeModeValLbl->setAnchorPoint({1.f, 0.5f});
        m_activeModeValLbl->setColor(COL_TEXT_SOFT);
        m_activeModeValLbl->setScale(0.48f);
        m_activeModeValLbl->setPosition({px(1480.f), py(372.f)});
        m_mainLayer->addChild(m_activeModeValLbl, 6);

        // -------------------------------------------------------------------
        // PANEL 2: CHAT / COMANDO
        // Construido en coordenadas locales respecto a su propio tamano (396 x 268 ref)
        // y punto de anclaje {0.5, 0.5} para que al escalarse y moverse al centro
        // en la pestana CHAT quede 100% encuadrado sin desplazarse hacia la derecha.
        // -------------------------------------------------------------------
        const float cw = ps(396.f);
        const float ch = ps(268.f);
        m_chatPanelNode = CCNode::create();
        m_chatPanelNode->setContentSize({cw, ch});
        m_chatPanelNode->ignoreAnchorPointForPosition(false);
        m_chatPanelNode->setAnchorPoint({0.5f, 0.5f});
        m_chatPanelNode->setPosition({px(1304.f), py(554.f)});
        m_mainLayer->addChild(m_chatPanelNode, 20);

        auto chatBgDn = CCDrawNode::create();
        ui::drawRoundedRect(
            chatBgDn, 0.f, 0.f, cw, ch, ps(18.f),
            rgba4F(COL_BG_RIGHT_CARD, 0.96f),
            ps(2.2f),
            rgba4F(COL_FRAME_BLUE, 0.92f)
        );
        // Icono de burbuja con 3 puntos en el encabezado local
        ui::drawRoundedRect(
            chatBgDn, ps(18.f), ch - ps(36.f), ps(28.f), ps(20.f), ps(6.f),
            rgba4F(COL_TEXT_CYAN, 1.f), 0.f, ccc4f(0, 0, 0, 0)
        );
        for (int d = -1; d <= 1; ++d) {
            chatBgDn->drawDot(ccp(ps(32.f + d * 6.f), ch - ps(26.f)), ps(2.0f), rgba4F(COL_BG_RIGHT_CARD, 1.f));
        }
        chatBgDn->drawSegment(ccp(ps(16.f), ch - ps(52.f)), ccp(cw - ps(16.f), ch - ps(52.f)), ps(1.5f), divCol);

        // Caja interna de entrada de texto
        ui::drawRoundedRect(
            chatBgDn, ps(20.f), ch - ps(198.f), ps(358.f), ps(136.f), ps(14.f),
            rgba4F(COL_BG_INPUT, 0.98f),
            ps(1.8f),
            rgba4F(ccc3(0x16, 0x3b, 0x82), 0.92f)
        );
        m_chatPanelNode->addChild(chatBgDn, 1);

        auto chatTitle = CCLabelBMFont::create("CHAT / COMANDO", "bigFont.fnt");
        chatTitle->setAnchorPoint({0.f, 0.5f});
        chatTitle->setColor(COL_TEXT_CYAN);
        chatTitle->setPosition({ps(58.f), ch - ps(26.f)});
        chatTitle->limitLabelWidth(ps(310.f), 0.34f, 0.18f);
        m_chatPanelNode->addChild(chatTitle, 3);

        m_placeholderNode = CCNode::create();
        m_chatPanelNode->addChild(m_placeholderNode, 4);
        char const* phLines[4] = {
            "Escribe lo que quieres que haga",
            "en tu nivel... (Ej. \"Haz una transicion",
            "entre 2 zonas\", \"Mejora la deco de",
            "esta parte\", etc.)"
        };
        for (int l = 0; l < 4; ++l) {
            auto pl = CCLabelBMFont::create(phLines[l], "chatFont.fnt");
            pl->setAnchorPoint({0.f, 0.5f});
            pl->setColor(COL_TEXT_DIM);
            pl->setPosition({ps(38.f), ch - ps(88.f + static_cast<float>(l) * 28.f)});
            fitBMFontLabel(pl, phLines[l], ps(324.f), 0.45f);
            m_placeholderNode->addChild(pl);
        }

        for (int l = 0; l < 3; ++l) {
            m_typedPreviewLines[l] = CCLabelBMFont::create("", "chatFont.fnt");
            m_typedPreviewLines[l]->setAnchorPoint({0.f, 0.5f});
            m_typedPreviewLines[l]->setColor(COL_TEXT_WHITE);
            m_typedPreviewLines[l]->setPosition({ps(38.f), ch - ps(116.f + static_cast<float>(l) * 28.f)});
            m_chatPanelNode->addChild(m_typedPreviewLines[l], 5);
        }

        m_promptInput = TextInput::create(ps(336.f), " ", "chatFont.fnt");
        m_promptInput->setCommonFilter(CommonFilter::Any);
        m_promptInput->setMaxCharCount(160);
        m_promptInput->setPosition({cw * 0.5f, ch - ps(90.f)});
        if (m_promptInput->getBGSprite()) {
            m_promptInput->getBGSprite()->setOpacity(0);
        }
        m_promptInput->setCallback([this](std::string const& text) {
            this->onPromptTextChanged(text);
        });
        m_chatPanelNode->addChild(m_promptInput, 10);

        auto chatMenu = CCMenu::create();
        chatMenu->setPosition({0.f, 0.f});
        m_chatPanelNode->addChild(chatMenu, 15);

        float sendW = ps(66.f), sendH = ps(44.f);
        auto sendNode = CCNode::create();
        sendNode->setContentSize({sendW, sendH});
        auto sendDn = CCDrawNode::create();
        ui::drawRoundedRect(
            sendDn, 0.f, 0.f, sendW, sendH, ps(10.f),
            rgba4F(ccc3(0x10, 0x32, 0x77), 0.98f),
            ps(1.6f),
            rgba4F(ccc3(0x3a, 0x78, 0xd8), 0.95f)
        );
        if (auto planeSpr = ui::createFitSprite("ui-send.png"_spr, ps(28.f), ps(26.f), ccp(sendW * 0.5f, sendH * 0.5f))) {
            planeSpr->setColor(ccc3(0x98, 0xc8, 0xff));
            sendNode->addChild(planeSpr, 2);
        } else {
            CCPoint plane[3] = {
                ccp(sendW * 0.28f, sendH * 0.74f),
                ccp(sendW * 0.76f, sendH * 0.50f),
                ccp(sendW * 0.28f, sendH * 0.26f)
            };
            sendDn->drawPolygon(plane, 3, rgba4F(ccc3(0x98, 0xc8, 0xff), 1.f), 0.f, ccc4f(0, 0, 0, 0));
        }
        sendNode->addChild(sendDn);

        auto sendBtn = CCMenuItemExt::createSpriteExtra(
            sendNode, [this](CCMenuItemSpriteExtra*) { this->onSendCommand(); }
        );
        sendBtn->m_scaleMultiplier = 1.03f;
        sendBtn->setPosition({cw - ps(51.f), ps(38.f)});
        chatMenu->addChild(sendBtn);

        // -------------------------------------------------------------------
        // PANEL 3: HISTORIAL
        // Sin botones al lado de cada oracion: un unico boton delgado en la
        // esquina superior derecha del recuadro despliega la ventana de historial.
        // -------------------------------------------------------------------
        m_histPanelNode = CCNode::create();
        m_histPanelNode->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_histPanelNode, 18);

        ui::addRefRoundedBox(
            m_histPanelNode,
            1106.f, 702.f, 396.f, 178.f, 18.f,
            COL_BG_RIGHT_CARD, 0.96f,
            COL_FRAME_BLUE, 0.90f, 2.2f, 1
        );

        auto histDn = CCDrawNode::create();
        if (auto clockSpr = ui::createFitSprite("ui-clock.png"_spr, ps(28.f), ps(28.f), ccp(px(1138.f), py(728.f)))) {
            clockSpr->setColor(COL_TEXT_CYAN);
            m_histPanelNode->addChild(clockSpr, 3);
        } else {
            ui::drawCircleShape(
                histDn, ccp(px(1138.f), py(728.f)), ps(13.f),
                rgba4F(COL_TEXT_CYAN, 1.f), 0.f, ccc4f(0, 0, 0, 0), 18
            );
        }

        histDn->drawSegment(ccp(px(1122.f), py(752.f)), ccp(px(1486.f), py(752.f)), ps(1.4f), divCol);
        histDn->drawSegment(ccp(px(1122.f), py(792.f)), ccp(px(1486.f), py(792.f)), ps(1.0f), divCol);
        histDn->drawSegment(ccp(px(1122.f), py(832.f)), ccp(px(1486.f), py(832.f)), ps(1.0f), divCol);

        const float rowY[3] = {772.f, 812.f, 852.f};
        for (int r = 0; r < 3; ++r) {
            histDn->drawDot(ccp(px(1135.f), py(rowY[r])), ps(4.0f), rgba4F(ccc3(0x3a, 0x86, 0xe8), 1.f));
        }
        m_histPanelNode->addChild(histDn, 2);

        auto histTitle = CCLabelBMFont::create("HISTORIAL", "bigFont.fnt");
        histTitle->setAnchorPoint({0.f, 0.5f});
        histTitle->setColor(COL_TEXT_CYAN);
        histTitle->setPosition({px(1164.f), py(728.f)});
        histTitle->limitLabelWidth(ps(240.f), 0.34f, 0.18f);
        m_histPanelNode->addChild(histTitle, 3);

        // Unico boton delgado en la esquina superior derecha del recuadro HISTORIAL
        auto histMenu = CCMenu::create();
        histMenu->setPosition({0.f, 0.f});
        m_histPanelNode->addChild(histMenu, 10);

        auto openHistNode = ui::makeCustomMenuBtn(">", ps(34.f), ps(26.f), COL_BG_SIDE_IDLE, ccc3(0x2c, 0x6c, 0xc8), 0.26f);
        auto openHistBtn = CCMenuItemExt::createSpriteExtra(
            openHistNode, [this](CCMenuItemSpriteExtra*) { this->openFullHistoryModal(); }
        );
        openHistBtn->m_scaleMultiplier = 1.03f;
        openHistBtn->setPosition({px(1468.f), py(728.f)});
        histMenu->addChild(openHistBtn);

        // 3 filas de texto limpio sin botones a los lados
        for (int r = 0; r < 3; ++r) {
            m_histRowText[r] = CCLabelBMFont::create("", "chatFont.fnt");
            m_histRowText[r]->setAnchorPoint({0.f, 0.5f});
            m_histRowText[r]->setColor(COL_TEXT_SOFT);
            m_histRowText[r]->setPosition({px(1152.f), py(rowY[r])});
            m_histPanelNode->addChild(m_histRowText[r], 4);

            m_histRowTime[r] = CCLabelBMFont::create("", "chatFont.fnt");
            m_histRowTime[r]->setAnchorPoint({1.f, 0.5f});
            m_histRowTime[r]->setColor(COL_TEXT_DIM);
            m_histRowTime[r]->setPosition({px(1484.f), py(rowY[r])});
            m_histPanelNode->addChild(m_histRowTime[r], 4);
        }
    }

    void onPromptTextChanged(std::string const& text) {
        std::string clean = trimStr(sanitizeForGD(text));
        if (m_placeholderNode) {
            m_placeholderNode->setVisible(clean.empty());
        }
        for (int l = 0; l < 3; ++l) {
            if (m_typedPreviewLines[l]) m_typedPreviewLines[l]->setString("");
        }
        if (clean.size() > 38) {
            std::string rest = clean.substr(38);
            for (int l = 0; l < 3 && !rest.empty(); ++l) {
                std::string chunk = rest.substr(0, std::min<size_t>(38, rest.size()));
                fitBMFontLabel(m_typedPreviewLines[l], chunk, ps(324.f), 0.44f);
                if (rest.size() > 38) rest = rest.substr(38);
                else rest.clear();
            }
        }
    }

    void setPromptText(std::string const& text) {
        std::string clean = sanitizeForGD(text);
        if (m_promptInput) {
            m_promptInput->setString(clean);
        }
        this->onPromptTextChanged(clean);
    }

    // -----------------------------------------------------------------------
    // 9. BARRA INFERIOR Y BANNER DEL ALIADO
    // -----------------------------------------------------------------------
    void buildBottomBar() {
        ui::addRefRoundedBox(
            m_mainLayer,
            28.f, 898.f, 928.f, 78.f, 12.f,
            COL_BG_BOTTOM, 0.96f,
            COL_FRAME_BLUE, 0.88f, 2.0f, 3
        );

        auto botDn = CCDrawNode::create();
        const float divX[4] = {210.f, 392.f, 580.f, 768.f};
        for (int d = 0; d < 4; ++d) {
            botDn->drawSegment(
                ccp(px(divX[d]), py(908.f)),
                ccp(px(divX[d]), py(966.f)),
                ps(1.6f),
                rgba4F(COL_DIVIDER, 0.85f)
            );
        }
        m_mainLayer->addChild(botDn, 4);

        const float btnCentersX[5] = {118.f, 300.f, 486.f, 674.f, 862.f};
        for (int i = 0; i < 5; ++i) {
            float bw = ps(176.f), bh = ps(72.f);
            auto itemNode = CCNode::create();
            itemNode->setContentSize({bw, bh});

            if (auto icoSpr = ui::createBottomBarIconSprite(i, ccp(bw * 0.5f, bh * 0.64f))) {
                itemNode->addChild(icoSpr, 1);
            } else {
                auto iconDn = CCDrawNode::create();
                ui::drawBottomBarIcon(iconDn, i, ccp(bw * 0.5f, bh * 0.64f));
                itemNode->addChild(iconDn, 1);
            }

            auto lbl = CCLabelBMFont::create(BOTTOM_NAMES[i], "bigFont.fnt");
            lbl->setColor(COL_TEXT_SOFT);
            lbl->setPosition({bw * 0.5f, bh * 0.24f});
            lbl->limitLabelWidth(bw - ps(12.f), 0.24f, 0.14f);
            itemNode->addChild(lbl, 2);

            auto btn = CCMenuItemExt::createSpriteExtra(
                itemNode, [this, i](CCMenuItemSpriteExtra*) { this->onBottomBarAction(i); }
            );
            btn->m_scaleMultiplier = 1.03f;
            btn->setPosition({px(btnCentersX[i]), py(937.f)});
            m_fixedMenu->addChild(btn);
        }

        // Fondo fijo del banner del aliado y nodo de contorno tipo gusano RGB
        m_allyBgNode = CCDrawNode::create();
        ui::drawRoundedRect(
            m_allyBgNode,
            px(1090.f), py(972.f), ps(314.f), ps(74.f), ps(16.f),
            rgba4F(COL_BG_ALLY, 0.96f), 0.f, ccc4f(0, 0, 0, 0)
        );
        m_mainLayer->addChild(m_allyBgNode, 3);

        m_allyBorderNode = CCDrawNode::create();
        m_mainLayer->addChild(m_allyBorderNode, 4);

        auto allyDn = CCDrawNode::create();
        allyDn->drawSegment(
            ccp(px(1188.f), py(910.f)),
            ccp(px(1188.f), py(960.f)),
            ps(1.6f),
            rgba4F(COL_DIVIDER, 0.9f)
        );
        ui::drawSparkle4(allyDn, ccp(px(1418.f), py(935.f)), ps(13.f), rgba4F(ccc3(0x38, 0xd8, 0xff), 1.f));
        m_mainLayer->addChild(allyDn, 5);

        m_allyRobotNode = ui::createRobotMascotNode(false);
        m_allyRobotNode->ignoreAnchorPointForPosition(false);
        m_allyRobotNode->setAnchorPoint({0.5f, 0.5f});
        m_allyRobotNode->setPosition({px(1142.f), py(935.f)});
        m_mainLayer->addChild(m_allyRobotNode, 6);

        m_allyLine1 = CCLabelBMFont::create("!Tu aliado en el editor de", "chatFont.fnt");
        m_allyLine1->setAnchorPoint({0.f, 0.5f});
        m_allyLine1->setColor(COL_TEXT_SOFT);
        m_allyLine1->setPosition({px(1200.f), py(923.f)});
        fitBMFontLabel(m_allyLine1, "!Tu aliado en el editor de", ps(194.f), 0.44f);
        m_mainLayer->addChild(m_allyLine1, 6);

        m_allyLine2 = CCLabelBMFont::create("Geometry Dash!", "chatFont.fnt");
        m_allyLine2->setAnchorPoint({0.f, 0.5f});
        m_allyLine2->setColor(COL_TEXT_SOFT);
        m_allyLine2->setPosition({px(1200.f), py(948.f)});
        fitBMFontLabel(m_allyLine2, "Geometry Dash!", ps(194.f), 0.44f);
        m_mainLayer->addChild(m_allyLine2, 6);
    }

    void nextAllyPhrase() {
        if (m_phrases.empty()) m_phrases = HistoryManager::defaultPhrases();
        m_phraseIdx = (m_phraseIdx + 1) % m_phrases.size();
        m_phraseTimer = 0.f;
        this->updateAllyBannerText();
    }

    void updateAllyBannerText() {
        if (!m_allyLine1 || !m_allyLine2) return;
        if (m_phrases.empty()) m_phrases = HistoryManager::defaultPhrases();
        std::string phrase = sanitizeForGD(m_phrases[m_phraseIdx % m_phrases.size()]);

        std::string l1 = phrase;
        std::string l2;
        if (phrase.size() > 24) {
            size_t split = phrase.rfind(' ', 26);
            if (split == std::string::npos || split < 8) split = 24;
            l1 = trimStr(phrase.substr(0, split));
            l2 = trimStr(phrase.substr(split));
        }
        fitBMFontLabel(m_allyLine1, l1, ps(194.f), 0.44f);
        fitBMFontLabel(m_allyLine2, l2, ps(194.f), 0.44f);
    }

    // -----------------------------------------------------------------------
    // 10. INDICADORES, ENVIO DE COMANDO Y BUCLE DE FLUJO RGB TIPO GUSANO
    // -----------------------------------------------------------------------
    void refreshUsagePanel() {
        int used = Mod::get()->getSavedValue<int>("used", 0);
        int limit = Mod::get()->getSavedValue<int>("limit", 20);
        int total = Mod::get()->getSavedValue<int>("total", 0);
        if (limit < 1) limit = 20;

        if (m_quotaNumLbl) m_quotaNumLbl->setString(fmt::format("{}", used).c_str());
        if (m_quotaMaxLbl) m_quotaMaxLbl->setString(fmt::format("/ {}", limit).c_str());
        if (m_reqDoneValLbl) m_reqDoneValLbl->setString(fmt::format("{}", total).c_str());
        if (m_curLevelValLbl) {
            fitBMFontLabel(m_curLevelValLbl, LevelExecutor::getLevelTitle(m_lel), ps(155.f), 0.30f);
        }

        float frac = std::min(1.f, std::max(0.04f, static_cast<float>(used) / static_cast<float>(limit)));
        if (used == 0) frac = 0.06f;

        if (m_quotaBarFill) {
            m_quotaBarFill->clear();
            ui::drawRoundedRect(
                m_quotaBarFill,
                px(1138.f), py(255.f), ps(326.f * frac), ps(20.f), ps(10.f),
                rgba4F(COL_BAR_TEAL, 1.f), 0.f, ccc4f(0, 0, 0, 0)
            );
        }
    }

    void refreshHistoryMiniPanel() {
        auto const& h = HistoryManager::entries();
        for (int r = 0; r < 3; ++r) {
            int k = static_cast<int>(h.size()) - 1 - r;
            if (k >= 0) {
                auto const& entry = h[k];
                fitBMFontLabel(m_histRowText[r], entry.summary, ps(232.f), 0.44f);
                std::string tStr = HistoryManager::formatRelativeTime(entry.timestamp);
                fitBMFontLabel(m_histRowTime[r], tStr, ps(90.f), 0.42f);
            } else {
                fitBMFontLabel(
                    m_histRowText[r],
                    r == 0 ? "Sin peticiones recientes" : "-",
                    ps(232.f), 0.44f
                );
                fitBMFontLabel(m_histRowTime[r], "", ps(40.f), 0.42f);
            }
        }
    }

    void openFullHistoryModal() {
        auto popup = HistoryPopup::create([this]() {
            HistoryManager::clearHistory();
            this->refreshHistoryMiniPanel();
        });
        if (popup) popup->show();
    }

    void onBottomBarAction(int idx) {
        switch (idx) {
            case 0: {
                std::string curModeId = (m_selectedMode >= 0) ? MODES[m_selectedMode].id : "deco";
                std::string curPrompt = m_promptInput ? std::string(m_promptInput->getString().c_str()) : "";
                if (auto pop = FavoritesPopup::create(
                    curModeId, curPrompt,
                    [this](int mi, std::string const& p) {
                        if (mi >= 0) this->selectMode(mi);
                        this->setPromptText(p);
                    }
                )) {
                    pop->show();
                }
                break;
            }
            case 1: {
                if (auto pop = TutorialsPopup::create()) {
                    pop->show();
                }
                break;
            }
            case 2: {
                if (auto pop = LevelsBackupPopup::create(m_lel)) {
                    pop->show();
                }
                break;
            }
            case 3: {
                std::string msg;
                bool ok = LevelExecutor::saveLevelSnapshot(m_lel, msg);
                FLAlertLayer::create(ok ? "Guardado Completado" : "Aviso", msg, "OK")->show();
                break;
            }
            default: {
                if (m_aiClient.isBusy()) {
                    m_aiClient.cancel();
                }
                this->setPromptText("");
                this->selectMode(-1);
                this->showPage(0);
                break;
            }
        }
    }

    void updateChatStatusText(char const* statusTitle, ccColor3B statusCol, std::string const& body) {
        m_lastReplyText = sanitizeForGD(body);
        if (m_chatReplyStatusLbl) {
            m_chatReplyStatusLbl->setString(statusTitle);
            m_chatReplyStatusLbl->setColor(statusCol);
        }
        if (m_chatReplyBodyLbl) {
            fitBMFontLabel(m_chatReplyBodyLbl, m_lastReplyText, ps(660.f), 0.50f);
        }
    }

    void onSendCommand() {
        if (m_aiClient.isBusy()) {
            this->showPage(2);
            return;
        }

        std::string prompt = m_promptInput ? trimStr(std::string(m_promptInput->getString().c_str())) : "";
        if (prompt.empty()) {
            this->showPage(2);
            this->updateChatStatusText(
                "AVISO: COMANDO VACIO",
                ccc3(0xfb, 0xcf, 0x50),
                "Escribe primero una instruccion en la caja de texto antes de pulsar enviar."
            );
            return;
        }

        // Comando especial: /catalogo -> sube al servidor el catalogo de objetos del juego
        if (prompt == "/catalogo") {
            this->setPromptText("");
            this->showPage(2);
            this->updateChatStatusText(
                "CATALOGO DE OBJETOS",
                ccc3(0xfb, 0xcf, 0x50),
                "Leyendo los objetos del juego y enviandolos al servidor..."
            );
            m_aiClient.uploadCatalog([this](bool ok, std::string const& msg) {
                this->updateChatStatusText(
                    ok ? "CATALOGO ENVIADO" : "CATALOGO: REVISION REQUERIDA",
                    ok ? COL_NEON_GREEN : ccc3(0xf7, 0x54, 0x75),
                    msg
                );
            });
            return;
        }

        int modeToUse = (m_selectedMode >= 0 && m_selectedMode < 9) ? m_selectedMode : 2;
        if (m_selectedMode < 0) {
            this->selectMode(modeToUse);
        }

        this->setPromptText("");
        HistoryManager::addEntry(MODES[modeToUse].id, prompt);
        this->refreshHistoryMiniPanel();

        m_sessionRequests += 1;
        int total = Mod::get()->getSavedValue<int>("total", 0) + 1;
        Mod::get()->setSavedValue<int>("total", total);

        this->updateChatStatusText(
            "PROCESANDO SOLICITUD...",
            ccc3(0xfb, 0xcf, 0x50),
            fmt::format("Enviando peticion en modo {}: \"{}\"...", MODES[modeToUse].name, prompt)
        );

        m_aiClient.sendRequest(
            modeToUse,
            prompt,
            m_lel,
            [this, modeToUse](AIResponseData const& res) {
                if (!res.ok) {
                    this->updateChatStatusText("REVISION REQUERIDA", ccc3(0xf7, 0x54, 0x75), res.message);
                    this->refreshUsagePanel();
                    this->refreshStatsPage();
                    return;
                }

                if (res.usedToday >= 0 && res.dailyLimit > 0) {
                    Mod::get()->setSavedValue<int>("used", res.usedToday);
                    Mod::get()->setSavedValue<int>("limit", res.dailyLimit);
                }

                std::string finalMsg = res.message;
                if (!res.rawObjects.empty()) {
                    ValidationResult vr = LevelExecutor::applyValidatedObjects(
                        m_lel, res.rawObjects, MODES[modeToUse].id
                    );
                    if (vr.allowed && vr.validCount > 0) {
                        m_sessionPlacedObjects += vr.validCount;
                        finalMsg += fmt::format(" [{} objetos colocados].", vr.validCount);
                    } else if (!vr.note.empty()) {
                        finalMsg += " [" + vr.note + "]";
                    }
                }

                this->updateChatStatusText("RESPUESTA DE DECOMIND AI", COL_NEON_GREEN, finalMsg);
                this->refreshUsagePanel();
                this->refreshStatsPage();
            }
        );

        this->refreshUsagePanel();
        this->refreshStatsPage();
    }

    // Bucle de animacion: Flujo RGB tipo "gusano" de A hacia B, robot animado y fade de frases
    void tick(float dt) {
        m_animTime += dt;
        float rgbSpeed = ThemeConfig::getRgbSpeed();
        m_wormPhase += dt * 0.42f * rgbSpeed;

        if (m_mascotTop) {
            m_mascotTop->setPositionY(std::sin(m_animTime * 2.4f) * 1.6f);
        }
        if (m_allyRobotNode) {
            m_allyRobotNode->setPosition({
                px(1142.f),
                py(935.f) + std::sin(m_animTime * 3.6f) * 1.8f
            });
            m_allyRobotNode->setScale(1.0f + 0.04f * std::sin(m_animTime * 4.2f));
        }

        bool rgbOn = ThemeConfig::isRgbEnabled();
        const float cardW = ps(230.f);
        const float cardH = ps(198.f);

        // 1. Gusano RGB recorriendo el perimetro de cada una de las 9 tarjetas
        for (int i = 0; i < 9; ++i) {
            if (!m_cardBorderNodes[i]) continue;
            ui::drawWormPerimeterBorder(
                m_cardBorderNodes[i],
                0.f, 0.f, cardW, cardH, ps(15.f),
                MODES[i].borderColor,
                m_wormPhase + i * 0.11f,
                rgbOn,
                false, // Flujo dentro de la paleta de color propia de cada tarjeta
                ps(2.8f)
            );
        }

        // 2. Flujo RGB sobre los picos decorativos de Geometry Dash
        ui::redrawSpikesWormRgb(m_spikesRgbNode, m_wormPhase, rgbOn);

        // 3. Gusano RGB alrededor del banner inferior derecho del robot aliado
        ui::drawWormPerimeterBorder(
            m_allyBorderNode,
            px(1090.f), py(972.f), ps(314.f), ps(74.f), ps(16.f),
            ccc3(0x18, 0x82, 0xf2),
            m_wormPhase + 0.35f,
            rgbOn,
            true, // Flujo arcoiris RGB en el banner del aliado
            ps(2.6f)
        );

        // 4. Aparicion y desaparicion gradual (fade-in / fade-out) de las frases del robot
        const float cycleDuration = 6.5f;
        m_phraseTimer += dt;
        if (m_phraseTimer >= cycleDuration) {
            m_phraseTimer = 0.f;
            if (!m_phrases.empty()) {
                m_phraseIdx = (m_phraseIdx + 1) % m_phrases.size();
                this->updateAllyBannerText();
            }
        }

        float fadeFactor = 1.f;
        if (m_phraseTimer < 1.0f) {
            fadeFactor = m_phraseTimer / 1.0f;
        } else if (m_phraseTimer > (cycleDuration - 1.0f)) {
            fadeFactor = std::max(0.f, (cycleDuration - m_phraseTimer) / 1.0f);
        }
        ccColor3B fadedTextCol = lerpColor(COL_BG_ALLY, COL_TEXT_CYAN, fadeFactor);
        if (m_allyLine1) m_allyLine1->setColor(fadedTextCol);
        if (m_allyLine2) m_allyLine2->setColor(fadedTextCol);
    }

public:
    static DecoMindPopup* create(LevelEditorLayer* lel) {
        auto ret = new DecoMindPopup();
        if (ret->init(lel)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

} // namespace

// ===========================================================================
// Integracion en el Editor de Geometry Dash
// ===========================================================================
class $modify(DecoMindEditorUI, EditorUI) {
    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;

        CCNode* btnVisual = nullptr;
        if (auto customSpr = CCSprite::create("editor-btn.png"_spr)) {
            float targetSize = 34.f;
            float s = std::min(targetSize / customSpr->getContentWidth(), targetSize / customSpr->getContentHeight());
            customSpr->setScale(s);
            btnVisual = customSpr;
        } else {
            btnVisual = ButtonSprite::create("IA", "bigFont.fnt", "GJ_button_02.png", 0.8f);
        }

        auto btn = CCMenuItemSpriteExtra::create(
            btnVisual, this, menu_selector(DecoMindEditorUI::onOpenDecoMind)
        );
        auto menu = CCMenu::create();
        menu->addChild(btn);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition({win.width * 0.34f, win.height - 28.f});
        menu->setID("decomind-menu"_spr);
        this->addChild(menu, 100);
        return true;
    }

    void onOpenDecoMind(CCObject*) {
        if (auto popup = DecoMindPopup::create(m_editorLayer)) {
            popup->show();
        }
    }
};
