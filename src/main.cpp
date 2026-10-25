// ===========================================================================
// DecoMind AI v0.3.12 - Menu Interactivo Nativo para el Editor de Geometry Dash
//
// Novedades v0.3.12:
//   - Nuevo icono personalizado en el boton del editor (editor-btn.png).
//   - Cuadricula central de modos deslizante hacia abajo (ScrollLayer).
//   - Contorno RGB por paleta de colores y degradado interior mas claro en modos.
//   - Decoracion RGB en picos inferiores izquierdos y contorno RGB + robot
//     animado + frases con fade-in/fade-out gradual en el banner del aliado.
//   - Reubicacion animada en pestana CHAT: el cuadro CHAT / COMANDO se mueve
//     al centro y el cuadro HISTORIAL sube a ocupar su lugar en la derecha.
//   - Apertura de Historial unicamente mediante el boton '>' dedicado, y
//     ventana emergente de Historial deslizante de solo lectura.
//   - Pestana AJUSTES redisenada con selector de color estilo Editor de GD
//     para personalizar la paleta del menu y la velocidad/estado/color RGB.
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
// Clase base para ventanas secundarias con animacion de apertura suave
// ===========================================================================
class DMModalBase : public Popup {
protected:
    CCMenu* m_ui = nullptr;
    float m_w = 0.f;
    float m_h = 0.f;

    bool initModal(float w, float h, char const* titleText) {
        if (!Popup::init(w, h)) return false;
        m_w = w;
        m_h = h;

        m_bgSprite->setVisible(false);
        m_closeBtn->setVisible(false);

        ccColor3B acc = ThemeConfig::getMenuAccent();
        ccColor3B bgOuter = lerpColor(COL_BG_OUTER, acc, 0.14f);
        ccColor3B bgInner = lerpColor(COL_BG_FRAME, acc, 0.16f);

        auto dn = CCDrawNode::create();
        ui::drawRoundedRect(
            dn, 0.f, 0.f, w, h, 8.f,
            rgba4F(bgOuter, 0.98f),
            2.2f,
            rgba4F(lerpColor(acc, COL_FRAME_CYAN, 0.5f), 0.95f)
        );
        ui::drawRoundedRect(
            dn, 4.f, 4.f, w - 8.f, h - 8.f, 6.f,
            rgba4F(bgInner, 0.98f),
            1.2f,
            rgba4F(acc, 0.9f)
        );
        dn->drawSegment(ccp(12.f, h - 32.f), ccp(w - 12.f, h - 32.f), 1.f, rgba4F(acc, 0.8f));
        m_mainLayer->addChild(dn, 1);

        m_ui = CCMenu::create();
        m_ui->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_ui, 50);

        auto backIcon = ui::createBackArrowIcon();
        backIcon->setScale(0.68f);
        auto closeBtn = CCMenuItemExt::createSpriteExtra(
            backIcon, [this](CCMenuItemSpriteExtra*) { this->onClose(nullptr); }
        );
        closeBtn->setPosition({22.f, h - 17.f});
        m_ui->addChild(closeBtn);

        auto title = CCLabelBMFont::create(sanitizeForGD(titleText).c_str(), "bigFont.fnt");
        title->setColor(COL_TEXT_WHITE);
        title->setPosition({w * 0.5f, h - 17.f});
        title->limitLabelWidth(w - 80.f, 0.44f, 0.2f);
        m_mainLayer->addChild(title, 10);

        return true;
    }

    CCNode* makeStyledActionBtn(char const* text, float w, float h, ccColor3B fillCol, ccColor3B borderCol) {
        return ui::makeCustomMenuBtn(text, w, h, fillCol, borderCol, 0.32f);
    }
};

// ===========================================================================
// Selector de Colores Estilo Editor Oficial de Geometry Dash (ColorSelectPopup)
// ===========================================================================
class GDColorPickerPopup : public DMModalBase {
protected:
    ccColor3B m_origColor;
    ccColor3B m_curColor;
    ccColor3B m_defaultColor;
    std::function<void(ccColor3B)> m_onApply;

    CCDrawNode* m_previewNode = nullptr;
    CCDrawNode* m_barsNode = nullptr;
    CCLabelBMFont* m_rValLbl = nullptr;
    CCLabelBMFont* m_gValLbl = nullptr;
    CCLabelBMFont* m_bValLbl = nullptr;
    CCLabelBMFont* m_hexLbl = nullptr;

    bool init(
        char const* title,
        ccColor3B initialCol,
        ccColor3B defaultCol,
        std::function<void(ccColor3B)> onApply
    ) {
        if (!this->initModal(350.f, 250.f, title)) return false;
        m_origColor = initialCol;
        m_curColor = initialCol;
        m_defaultColor = defaultCol;
        m_onApply = std::move(onApply);

        // 1. Paleta superior de 18 colores clasicos del editor de Geometry Dash
        static const ccColor3B GD_PRESETS[18] = {
            ccc3(0x0a, 0x4c, 0xc4), ccc3(0x18, 0xc8, 0xff), ccc3(0x01, 0xd3, 0xae),
            ccc3(0x1e, 0xf0, 0x98), ccc3(0x70, 0xff, 0x4d), ccc3(0xfb, 0xcf, 0x50),
            ccc3(0xfa, 0x92, 0x37), ccc3(0xff, 0x52, 0x52), ccc3(0xf7, 0x54, 0x75),
            ccc3(0xff, 0x4d, 0xc4), ccc3(0xb6, 0x48, 0xf6), ccc3(0x7b, 0x3f, 0xe4),
            ccc3(0x4d, 0x5e, 0xff), ccc3(0x00, 0x9b, 0xf7), ccc3(0x00, 0xe3, 0xf7),
            ccc3(0x98, 0xc8, 0xff), ccc3(0x28, 0x42, 0x78), ccc3(0x12, 0x24, 0x4e)
        };

        auto presetLbl = CCLabelBMFont::create("Paleta rapida del Editor GD:", "chatFont.fnt");
        presetLbl->setAnchorPoint({0.f, 0.5f});
        presetLbl->setColor(COL_TEXT_SOFT);
        presetLbl->setScale(0.48f);
        presetLbl->setPosition({18.f, 204.f});
        m_mainLayer->addChild(presetLbl, 5);

        for (int i = 0; i < 18; ++i) {
            int row = i / 9;
            int col = i % 9;
            float sw = 22.f, sh = 18.f;
            auto swNode = CCNode::create();
            swNode->setContentSize({sw, sh});
            auto sdn = CCDrawNode::create();
            ui::drawRoundedRect(
                sdn, 0.f, 0.f, sw, sh, 4.f,
                rgba4F(GD_PRESETS[i], 1.f), 1.2f, rgba4F(COL_TEXT_WHITE, 0.85f)
            );
            swNode->addChild(sdn);

            ccColor3B picked = GD_PRESETS[i];
            auto btn = CCMenuItemExt::createSpriteExtra(swNode, [this, picked](CCMenuItemSpriteExtra*) {
                m_curColor = picked;
                this->refreshPreview();
            });
            btn->setPosition({29.f + col * 26.f, 184.f - row * 22.f});
            m_ui->addChild(btn);
        }

        // 2. Recuadro de vista previa Original vs Nuevo (como en ColorSelectPopup de GD)
        auto prevTitle = CCLabelBMFont::create("Vista previa", "chatFont.fnt");
        prevTitle->setColor(COL_TEXT_SOFT);
        prevTitle->setScale(0.46f);
        prevTitle->setPosition({298.f, 204.f});
        m_mainLayer->addChild(prevTitle, 5);

        m_previewNode = CCDrawNode::create();
        m_mainLayer->addChild(m_previewNode, 6);

        m_hexLbl = CCLabelBMFont::create("#000000", "bigFont.fnt");
        m_hexLbl->setScale(0.28f);
        m_hexLbl->setColor(COL_TEXT_CYAN);
        m_hexLbl->setPosition({298.f, 134.f});
        m_mainLayer->addChild(m_hexLbl, 6);

        // 3. Controles deslizantes R / G / B con barras de color
        m_barsNode = CCDrawNode::create();
        m_mainLayer->addChild(m_barsNode, 4);

        char const* chNames[3] = {"R", "G", "B"};
        ccColor3B chCols[3] = {ccc3(255, 85, 85), ccc3(85, 255, 130), ccc3(85, 180, 255)};
        for (int c = 0; c < 3; ++c) {
            float ry = 112.f - c * 26.f;
            auto clbl = CCLabelBMFont::create(chNames[c], "bigFont.fnt");
            clbl->setScale(0.34f);
            clbl->setColor(chCols[c]);
            clbl->setPosition({24.f, ry});
            m_mainLayer->addChild(clbl, 6);

            auto valLbl = CCLabelBMFont::create("255", "bigFont.fnt");
            valLbl->setScale(0.30f);
            valLbl->setColor(COL_TEXT_WHITE);
            valLbl->setPosition({228.f, ry});
            m_mainLayer->addChild(valLbl, 6);
            if (c == 0) m_rValLbl = valLbl;
            else if (c == 1) m_gValLbl = valLbl;
            else m_bValLbl = valLbl;

            // Botones -15, -1, +1, +15 para ajuste preciso y rapido
            struct StepBtn { char const* txt; int delta; float x; };
            StepBtn steps[4] = {{"<<", -16, 260.f}, {"-", -2, 284.f}, {"+", +2, 308.f}, {">>", +16, 332.f}};
            for (auto const& st : steps) {
                auto bNode = ui::makeCustomMenuBtn(st.txt, 21.f, 19.f, COL_BG_SIDE_IDLE, chCols[c], 0.24f);
                int ch = c;
                int d = st.delta;
                auto bItem = CCMenuItemExt::createSpriteExtra(bNode, [this, ch, d](CCMenuItemSpriteExtra*) {
                    this->adjustChannel(ch, d);
                });
                bItem->setPosition({st.x, ry});
                m_ui->addChild(bItem);
            }
        }

        // 4. Botones inferiores: Por defecto y Aplicar
        auto defSpr = this->makeStyledActionBtn("Color por defecto", 132.f, 24.f, COL_BG_SIDE_IDLE, COL_TEXT_SOFT);
        auto defBtn = CCMenuItemExt::createSpriteExtra(defSpr, [this](CCMenuItemSpriteExtra*) {
            m_curColor = m_defaultColor;
            this->refreshPreview();
        });
        defBtn->setPosition({105.f, 24.f});
        m_ui->addChild(defBtn);

        auto okSpr = this->makeStyledActionBtn("Aplicar color", 132.f, 24.f, ccc3(0x06, 0x38, 0x44), COL_NEON_GREEN);
        auto okBtn = CCMenuItemExt::createSpriteExtra(okSpr, [this](CCMenuItemSpriteExtra*) {
            if (m_onApply) m_onApply(m_curColor);
            this->onClose(nullptr);
        });
        okBtn->setPosition({252.f, 24.f});
        m_ui->addChild(okBtn);

        this->refreshPreview();
        return true;
    }

    void adjustChannel(int ch, int delta) {
        int r = m_curColor.r, g = m_curColor.g, b = m_curColor.b;
        if (ch == 0) r = std::clamp(r + delta, 0, 255);
        else if (ch == 1) g = std::clamp(g + delta, 0, 255);
        else b = std::clamp(b + delta, 0, 255);
        m_curColor = ccc3(static_cast<GLubyte>(r), static_cast<GLubyte>(g), static_cast<GLubyte>(b));
        this->refreshPreview();
    }

    void refreshPreview() {
        if (m_previewNode) {
            m_previewNode->clear();
            // Mitad izquierda: color original; mitad derecha: color nuevo
            ui::drawRoundedRect(
                m_previewNode, 266.f, 148.f, 32.f, 44.f, 5.f,
                rgba4F(m_origColor, 1.f), 1.5f, rgba4F(COL_TEXT_SOFT, 0.9f)
            );
            ui::drawRoundedRect(
                m_previewNode, 298.f, 148.f, 32.f, 44.f, 5.f,
                rgba4F(m_curColor, 1.f), 1.8f, rgba4F(COL_TEXT_WHITE, 1.f)
            );
        }

        if (m_barsNode) {
            m_barsNode->clear();
            int vals[3] = {m_curColor.r, m_curColor.g, m_curColor.b};
            ccColor3B cols[3] = {
                ccc3(m_curColor.r, 40, 40),
                ccc3(40, m_curColor.g, 80),
                ccc3(40, 110, m_curColor.b)
            };
            for (int c = 0; c < 3; ++c) {
                float ry = 112.f - c * 26.f;
                float barX = 40.f, barW = 160.f, barH = 10.f;
                ui::drawRoundedRect(
                    m_barsNode, barX, ry - barH * 0.5f, barW, barH, 4.f,
                    rgba4F(ccc3(0x05, 0x12, 0x30), 1.f), 1.f, rgba4F(COL_DIVIDER, 0.9f)
                );
                float fillW = std::max(4.f, barW * (vals[c] / 255.f));
                ui::drawRoundedRect(
                    m_barsNode, barX, ry - barH * 0.5f, fillW, barH, 4.f,
                    rgba4F(cols[c], 1.f), 0.f, ccc4f(0, 0, 0, 0)
                );
            }
        }

        if (m_rValLbl) m_rValLbl->setString(fmt::format("{}", static_cast<int>(m_curColor.r)).c_str());
        if (m_gValLbl) m_gValLbl->setString(fmt::format("{}", static_cast<int>(m_curColor.g)).c_str());
        if (m_bValLbl) m_bValLbl->setString(fmt::format("{}", static_cast<int>(m_curColor.b)).c_str());
        if (m_hexLbl) {
            m_hexLbl->setString(fmt::format(
                "#{:02X}{:02X}{:02X}",
                static_cast<int>(m_curColor.r),
                static_cast<int>(m_curColor.g),
                static_cast<int>(m_curColor.b)
            ).c_str());
        }
    }

public:
    static GDColorPickerPopup* create(
        char const* title,
        ccColor3B initialCol,
        ccColor3B defaultCol,
        std::function<void(ccColor3B)> onApply
    ) {
        auto ret = new GDColorPickerPopup();
        if (ret->init(title, initialCol, defaultCol, std::move(onApply))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ===========================================================================
// Ventana Secundaria 1: Historial de Peticiones (Solo lectura + Scroll hacia abajo)
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
            rgba4F(COL_BG_CENTER, 0.95f), 1.2f, rgba4F(acc, 0.8f)
        );
        m_mainLayer->addChild(boxDn, 2);

        m_scroll = ScrollLayer::create(CCSize{304.f, 146.f});
        m_scroll->ignoreAnchorPointForPosition(false);
        m_scroll->setPosition({170.f, 122.f});
        m_mainLayer->addChild(m_scroll, 5);

        auto clearSpr = this->makeStyledActionBtn("BORRAR HISTORIAL", 142.f, 24.f, ccc3(0x3a, 0x10, 0x28), ccc3(0xf7, 0x54, 0x75));
        auto clearBtn = CCMenuItemExt::createSpriteExtra(clearSpr, [this](CCMenuItemSpriteExtra*) {
            if (m_onClear) m_onClear();
            this->buildList();
        });
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
            emptyLbl->setScale(0.58f);
            emptyLbl->setColor(COL_TEXT_SOFT);
            emptyLbl->setPosition({viewW * 0.5f, viewH * 0.5f});
            layer->addChild(emptyLbl);
            m_scroll->scrollToTop();
            return;
        }

        float itemH = 38.f;
        float gap = 5.f;
        float total = 8.f + static_cast<float>(h.size()) * (itemH + gap);
        total = std::max(viewH, total);
        layer->setContentSize({viewW, total});

        // Recuadros informativos de solo lectura (sin botones clicables que bloqueen el deslizamiento)
        int row = 0;
        for (int k = static_cast<int>(h.size()) - 1; k >= 0; --k, ++row) {
            auto const& e = h[k];
            int mi = modeIndexById(e.modeId);
            ccColor3B accent = (mi >= 0) ? MODES[mi].borderColor : COL_FRAME_CYAN;
            std::string modeName = (mi >= 0) ? MODES[mi].title : "GENERAL";
            std::string relTime = HistoryManager::formatRelativeTime(e.timestamp);

            auto node = CCNode::create();
            node->setContentSize({294.f, itemH});
            node->setPosition({5.f, total - 4.f - static_cast<float>(row) * (itemH + gap) - itemH});

            auto dn = CCDrawNode::create();
            ui::drawRoundedRectWithSoftGradient(
                dn, 0.f, 0.f, 294.f, itemH, 5.f,
                COL_BG_RIGHT_CARD, accent, 0.96f, 1.3f, rgba4F(accent, 0.90f)
            );
            dn->drawDot(ccp(12.f, itemH * 0.5f), 3.2f, rgba4F(accent, 1.f));
            node->addChild(dn, 1);

            auto lMode = CCLabelBMFont::create("", "bigFont.fnt");
            lMode->setAnchorPoint({0.f, 0.5f});
            lMode->setColor(accent);
            lMode->setPosition({22.f, 26.f});
            fitBMFontLabel(lMode, modeName, 175.f, 0.28f);
            node->addChild(lMode, 2);

            auto lTime = CCLabelBMFont::create("", "chatFont.fnt");
            lTime->setAnchorPoint({1.f, 0.5f});
            lTime->setColor(COL_TEXT_SOFT);
            lTime->setPosition({284.f, 26.f});
            fitBMFontLabel(lTime, relTime, 90.f, 0.48f);
            node->addChild(lTime, 2);

            auto lPrompt = CCLabelBMFont::create("", "chatFont.fnt");
            lPrompt->setAnchorPoint({0.f, 0.5f});
            lPrompt->setColor(COL_TEXT_WHITE);
            lPrompt->setPosition({22.f, 11.f});
            fitBMFontLabel(lPrompt, e.prompt, 262.f, 0.52f);
            node->addChild(lPrompt, 2);

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
            rgba4F(COL_BG_CENTER, 0.95f), 1.2f, rgba4F(ThemeConfig::getMenuAccent(), 0.8f)
        );
        m_mainLayer->addChild(boxDn, 2);

        m_scroll = ScrollLayer::create(CCSize{304.f, 146.f});
        m_scroll->ignoreAnchorPointForPosition(false);
        m_scroll->setPosition({170.f, 122.f});
        m_mainLayer->addChild(m_scroll, 5);

        auto saveSpr = this->makeStyledActionBtn(
            "Guardar comando actual en Favoritos", 220.f, 24.f, COL_BG_SIDE_IDLE, COL_FRAME_CYAN
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

        float itemH = 38.f;
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
                rgba4F(COL_BG_RIGHT_CARD, 0.96f), 1.2f, rgba4F(accent, 0.85f)
            );
            node->addChild(dn, 1);

            auto l1 = CCLabelBMFont::create("", "bigFont.fnt");
            l1->setAnchorPoint({0.f, 0.5f});
            l1->setColor(accent);
            l1->setPosition({10.f, 26.f});
            fitBMFontLabel(l1, modeName, 236.f, 0.30f);
            node->addChild(l1, 2);

            auto l2 = CCLabelBMFont::create("", "chatFont.fnt");
            l2->setAnchorPoint({0.f, 0.5f});
            l2->setColor(COL_TEXT_WHITE);
            l2->setPosition({10.f, 11.f});
            fitBMFontLabel(l2, f.prompt, 236.f, 0.50f);
            node->addChild(l2, 2);

            auto menu = CCMenu::create();
            menu->setContentSize({viewW, itemH});
            menu->setPosition({0.f, total - 4.f - static_cast<float>(i) * (itemH + gap) - itemH});

            std::string promptCopy = f.prompt;
            auto pickBtn = CCMenuItemExt::createSpriteExtra(node, [this, mi, promptCopy](CCMenuItemSpriteExtra*) {
                if (m_onPick) m_onPick(mi, promptCopy);
                this->onClose(nullptr);
            });
            pickBtn->setPosition({134.f, itemH * 0.5f});
            menu->addChild(pickBtn);

            auto delNode = this->makeStyledActionBtn("X", 28.f, itemH - 4.f, ccc3(0x42, 0x12, 0x28), ccc3(0xf7, 0x54, 0x75));
            auto delBtn = CCMenuItemExt::createSpriteExtra(delNode, [this, i](CCMenuItemSpriteExtra*) {
                HistoryManager::removeFavorite(i);
                this->buildList();
            });
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
// Ventana Secundaria 3: Tutoriales y Manual Rapido
// ===========================================================================
class TutorialsPopup : public DMModalBase {
protected:
    bool init() {
        if (!this->initModal(350.f, 240.f, "TUTORIALES Y GUIA RAPIDA")) return false;

        auto boxDn = CCDrawNode::create();
        ui::drawRoundedRect(
            boxDn, 12.f, 16.f, 326.f, 184.f, 6.f,
            rgba4F(COL_BG_CENTER, 0.95f), 1.2f, rgba4F(ThemeConfig::getMenuAccent(), 0.8f)
        );
        m_mainLayer->addChild(boxDn, 2);

        auto scroll = ScrollLayer::create(CCSize{310.f, 172.f});
        scroll->ignoreAnchorPointForPosition(false);
        scroll->setPosition({175.f, 108.f});
        m_mainLayer->addChild(scroll, 5);

        std::string guideText =
            "1. COMO USAR DECOMIND AI EN EL EDITOR\n"
            "- En la pestana INICIO, desliza y elige uno de los 9 modos haciendo clic en su tarjeta.\n"
            "- Escribe tu instruccion en CHAT / COMANDO (o abre la pestana CHAT para llevar el cuadro al centro).\n"
            "- Pulsa el boton de envio (avion de papel) para ejecutar la solicitud.\n\n"
            "2. LOS 9 MODOS ESPECIALIZADOS\n"
            "- Estructurar: organiza grupos, capas y triggers sin cambiar colisiones.\n"
            "- Layout: crea estructuras y saltos jugables equilibrados.\n"
            "- Deco: anade decoracion respetando los bloques solidos.\n"
            "- Layout + Deco: construye jugabilidad y decoracion en dos pasadas.\n"
            "- Block Design: disena estructuras y bloques personalizados.\n"
            "- Triggers: configura triggers automaticos en grupos libres.\n"
            "- Transiciones: conecta dos zonas o cambios de estilo suavemente.\n"
            "- Efectos Avanzados: anade movimiento, pulsos y parallax.\n"
            "- Ideas y Sugerencias: analiza el nivel y ofrece consejos sin modificar objetos.\n\n"
            "3. PERSONALIZACION Y SERVIDOR IA\n"
            "- En la pestana AJUSTES puedes alternar entre Origen Local y Servidor HTTP (Groq / Python).\n"
            "- Tambien puedes personalizar la paleta de colores del menu y la velocidad/estado de los bordes RGB.";

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
            rgba4F(COL_BG_CENTER, 0.95f), 1.2f, rgba4F(ThemeConfig::getMenuAccent(), 0.8f)
        );
        m_mainLayer->addChild(boxDn, 2);

        m_scroll = ScrollLayer::create(CCSize{304.f, 128.f});
        m_scroll->ignoreAnchorPointForPosition(false);
        m_scroll->setPosition({170.f, 113.f});
        m_mainLayer->addChild(m_scroll, 5);

        auto snapSpr = this->makeStyledActionBtn("Crear punto de guardado ahora", 190.f, 24.f, COL_BG_SIDE_IDLE, COL_NEON_GREEN);
        auto snapBtn = CCMenuItemExt::createSpriteExtra(snapSpr, [this](CCMenuItemSpriteExtra*) {
            std::string msg;
            LevelExecutor::saveLevelSnapshot(m_lel, msg);
            this->buildList();
        });
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
                rgba4F(COL_BG_RIGHT_CARD, 0.96f), 1.f, rgba4F(ThemeConfig::getMenuAccent(), 0.85f)
            );
            node->addChild(dn, 1);

            auto l1 = CCLabelBMFont::create("", "bigFont.fnt");
            l1->setAnchorPoint({0.f, 0.5f});
            l1->setColor(COL_TEXT_WHITE);
            l1->setPosition({10.f, 22.f});
            fitBMFontLabel(l1, fmt::format("{} ({} obj)", s.levelName, s.objectCount), 274.f, 0.30f);
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
// Menu Principal de DecoMind AI v0.3.12
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

    // Nodos de marco y decoracion RGB
    CCDrawNode* m_outerFrameNode = nullptr;
    CCDrawNode* m_centerBoxNode = nullptr;
    CCDrawNode* m_spikesRgbNode = nullptr;

    // Contenedores de las 5 vistas centrales
    CCNode* m_centerPages[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};

    // Header superior
    CCNode* m_mascotTop = nullptr;
    CCDrawNode* m_topBarFill = nullptr;
    float m_animTime = 0.f;
    float m_rgbPhase = 0.f;

    // Pagina 0 (INICIO - Cuadricula 3x3 deslizante hacia abajo + contornos RGB por paleta)
    ScrollLayer* m_inicioScroll = nullptr;
    CCDrawNode* m_cardBorderNodes[9] = {nullptr};
    CCDrawNode* m_cardSelectionGlow[9] = {nullptr};

    // Pagina 1 (MODOS)
    CCLabelBMFont* m_modePageTitle = nullptr;
    CCLabelBMFont* m_modePageSubtitle = nullptr;
    SimpleTextArea* m_modePageGuide = nullptr;
    CCLabelBMFont* m_modePageExample = nullptr;

    // Pagina 2 (CHAT: muestra la respuesta de la IA abajo mientras CHAT / COMANDO se mueve al centro)
    CCLabelBMFont* m_chatReplyStatusLbl = nullptr;
    CCLabelBMFont* m_chatReplyBodyLbl = nullptr;

    // Pagina 3 (ESTADISTICAS)
    CCLabelBMFont* m_statsLabels[7] = {nullptr};

    // Pagina 4 (AJUSTES redisenado sin solapamientos + selectores de color GD)
    CCLabelBMFont* m_originValueLbl = nullptr;
    TextInput* m_serverUrlInput = nullptr;
    CCLabelBMFont* m_sectionsValueLbl = nullptr;
    CCDrawNode* m_themeSwatchNode = nullptr;
    CCLabelBMFont* m_rgbStateValueLbl = nullptr;
    CCLabelBMFont* m_rgbSpeedValueLbl = nullptr;
    CCDrawNode* m_rgbSwatchNode = nullptr;
    CCLabelBMFont* m_settingsStatusLbl = nullptr;

    // Columna Derecha - Panel 1: USO DE LA IA
    CCDrawNode* m_usoPanelBg = nullptr;
    CCDrawNode* m_quotaBarFill = nullptr;
    CCLabelBMFont* m_quotaNumLbl = nullptr;
    CCLabelBMFont* m_quotaMaxLbl = nullptr;
    CCLabelBMFont* m_reqDoneValLbl = nullptr;
    CCLabelBMFont* m_curLevelValLbl = nullptr;
    CCLabelBMFont* m_activeModeValLbl = nullptr;

    // Columna Derecha - Panel 2: CHAT / COMANDO (contenedor movil que se traslada al centro en la pestana CHAT)
    CCNode* m_chatPanelNode = nullptr;
    CCDrawNode* m_chatPanelBg = nullptr;
    TextInput* m_promptInput = nullptr;
    CCNode* m_placeholderNode = nullptr;
    CCLabelBMFont* m_typedPreviewLines[3] = {nullptr, nullptr, nullptr};

    // Columna Derecha - Panel 3: HISTORIAL (contenedor movil que sube cuando CHAT / COMANDO pasa al centro)
    CCNode* m_histPanelNode = nullptr;
    CCDrawNode* m_histPanelBg = nullptr;
    CCLabelBMFont* m_histRowText[3] = {nullptr, nullptr, nullptr};
    CCLabelBMFont* m_histRowTime[3] = {nullptr, nullptr, nullptr};

    // Barra Inferior Derecha - Banner del Aliado Robot (Contorno RGB + Robot animado + Fade de frases)
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

        // 1. Marco exterior sci-fi y picos RGB
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

    // Actualiza en tiempo real los colores del marco y contenedores segun la paleta elegida en Ajustes
    void refreshThemeVisuals() {
        ccColor3B acc = ThemeConfig::getMenuAccent();
        ccColor3B centerBg = lerpColor(COL_BG_CENTER, acc, 0.16f);

        ui::drawSciFiMainFrame(m_outerFrameNode, acc);

        if (m_centerBoxNode) {
            m_centerBoxNode->clear();
            ui::drawRoundedRect(
                m_centerBoxNode,
                px(308.f), py(844.f), ps(782.f), ps(681.f), ps(22.f),
                rgba4F(centerBg, 0.96f),
                ps(3.0f),
                rgba4F(acc, 0.95f)
            );
        }
        if (m_themeSwatchNode) {
            m_themeSwatchNode->clear();
            ui::drawRoundedRect(
                m_themeSwatchNode, 0.f, 0.f, ps(44.f), ps(26.f), ps(6.f),
                rgba4F(acc, 1.f), 1.5f, rgba4F(COL_TEXT_WHITE, 0.95f)
            );
        }
        if (m_rgbSwatchNode) {
            m_rgbSwatchNode->clear();
            ccColor3B rgbC = ThemeConfig::getCustomRgbColor();
            ui::drawRoundedRect(
                m_rgbSwatchNode, 0.f, 0.f, ps(44.f), ps(26.f), ps(6.f),
                rgba4F(rgbC, 1.f), 1.5f, rgba4F(COL_TEXT_WHITE, 0.95f)
            );
        }
        this->buildSidebar();
    }

    // -----------------------------------------------------------------------
    // 1. HEADER SUPERIOR
    // -----------------------------------------------------------------------
    void buildTopHeader() {
        auto backNode = ui::createBackArrowIcon();
        auto backBtn = CCMenuItemExt::createSpriteExtra(
            backNode, [this](CCMenuItemSpriteExtra*) { this->onClose(nullptr); }
        );
        backBtn->setPosition({px(72.5f), py(62.f)});
        m_fixedMenu->addChild(backBtn);

        m_mascotTop = ui::createRobotMascotNode(true);
        auto mascotBtn = CCMenuItemExt::createSpriteExtra(
            m_mascotTop, [this](CCMenuItemSpriteExtra*) { this->nextAllyPhrase(); }
        );
        mascotBtn->setPosition({px(828.f), py(80.f)});
        m_fixedMenu->addChild(mascotBtn);

        ui::addRefRoundedBox(
            m_mainLayer,
            1058.f, 34.f, 358.f, 54.f, 14.f,
            ccc3(0x03, 0x13, 0x3c), 0.96f,
            ccc3(0x12, 0x4c, 0xaa), 0.95f,
            2.8f, 3
        );

        auto topCapsDn = CCDrawNode::create();
        ui::drawSparkle4(topCapsDn, ccp(px(1092.f), py(61.f)), ps(15.f), rgba4F(ccc3(0x38, 0xf2, 0xd8), 1.f));
        ui::drawRoundedRect(
            topCapsDn,
            px(1126.f), py(67.f), ps(270.f), ps(11.f), ps(5.f),
            rgba4F(ccc3(0x06, 0x1c, 0x46), 1.f), 0.f, ccc4f(0, 0, 0, 0)
        );
        m_mainLayer->addChild(topCapsDn, 4);

        m_topBarFill = CCDrawNode::create();
        m_mainLayer->addChild(m_topBarFill, 5);

        auto diamondNode = ui::createTopDiamondButtonNode();
        auto diamondBtn = CCMenuItemExt::createSpriteExtra(
            diamondNode, [this](CCMenuItemSpriteExtra*) { this->showPage(3); }
        );
        diamondBtn->setPosition({px(1481.f), py(60.f)});
        m_fixedMenu->addChild(diamondBtn);
    }

    // -----------------------------------------------------------------------
    // 2. BARRA LATERAL IZQUIERDA (5 Pestanas)
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
            btn->setPosition({px(163.f), py(yCenters[i])});
            m_sidebarMenu->addChild(btn);
        }
    }

    // Cambia de pestana y ejecuta la reubicacion animada de CHAT / COMANDO y HISTORIAL
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

        // Reubicacion dinamica cuando se selecciona CHAT (Imagen 6):
        // - En CHAT (p == 2): el cuadro CHAT / COMANDO se mueve al centro (y se amplia),
        //   y el cuadro HISTORIAL sube a ocupar la posicion donde antes estaba el chat.
        // - En las demas pestanas: ambos regresan a sus posiciones originales en la columna derecha.
        if (m_chatPanelNode && m_histPanelNode) {
            if (p == 2) {
                m_chatPanelNode->setPosition({px(-570.f), py(1024.f - 95.f)});
                m_chatPanelNode->setScale(1.26f);
                m_histPanelNode->setPosition({0.f, ps(278.f)});
            } else {
                m_chatPanelNode->setPosition({0.f, 0.f});
                m_chatPanelNode->setScale(1.0f);
                m_histPanelNode->setPosition({0.f, 0.f});
            }
        }
    }

    // -----------------------------------------------------------------------
    // 3. PAGINA 0 (INICIO): CUADRICULA 3x3 DESLIZANTE + RGB POR PALETA + DEGRADADO
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
        // Altura deslizable hacia abajo para alojar las 9 tarjetas y futuras expansiones
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

            // Fondo con degradado leve hacia un tono mas claro de su propia paleta (Imagen 5)
            auto bgDn = CCDrawNode::create();
            ui::drawRoundedRectWithSoftGradient(
                bgDn, 0.f, 0.f, cardW, cardH, ps(15.f),
                m.bgColor, m.borderColor, 0.96f,
                0.f, ccc4f(0, 0, 0, 0)
            );
            cardNode->addChild(bgDn, 1);

            // Nodo de contorno RGB animado por la paleta de colores de la tarjeta
            auto borderDn = CCDrawNode::create();
            ui::drawRoundedRect(
                borderDn, 0.f, 0.f, cardW, cardH, ps(15.f),
                ccc4f(0, 0, 0, 0), ps(3.2f), rgba4F(m.borderColor, 1.f)
            );
            cardNode->addChild(borderDn, 2);
            m_cardBorderNodes[i] = borderDn;

            // Resaltado adicional cuando el modo esta activo
            auto selGlow = CCDrawNode::create();
            ui::drawRoundedRect(
                selGlow, ps(3.f), ps(3.f), cardW - ps(6.f), cardH - ps(6.f), ps(12.f),
                rgba4F(m.borderColor, 0.18f),
                ps(2.2f),
                rgba4F(COL_TEXT_WHITE, 0.95f)
            );
            selGlow->setVisible(false);
            cardNode->addChild(selGlow, 3);
            m_cardSelectionGlow[i] = selGlow;

            auto iconNode = ui::createModeIconNode(i);
            iconNode->ignoreAnchorPointForPosition(false);
            iconNode->setAnchorPoint({0.5f, 0.5f});
            iconNode->setPosition({cardW * 0.5f, cardH * 0.67f});
            cardNode->addChild(iconNode, 4);

            auto titleLbl = CCLabelBMFont::create(m.title, "bigFont.fnt");
            titleLbl->setColor(COL_TEXT_WHITE);
            titleLbl->setPosition({cardW * 0.5f, cardH * 0.36f});
            titleLbl->limitLabelWidth(cardW - ps(16.f), 0.30f, 0.14f);
            cardNode->addChild(titleLbl, 5);

            auto desc1 = CCLabelBMFont::create(m.visibleLine1, "chatFont.fnt");
            desc1->setColor(ccc3(0xe2, 0xf2, 0xff));
            desc1->setPosition({cardW * 0.5f, cardH * 0.21f});
            desc1->limitLabelWidth(cardW - ps(14.f), 0.44f, 0.18f);
            cardNode->addChild(desc1, 5);

            auto desc2 = CCLabelBMFont::create(m.visibleLine2, "chatFont.fnt");
            desc2->setColor(ccc3(0xe2, 0xf2, 0xff));
            desc2->setPosition({cardW * 0.5f, cardH * 0.11f});
            desc2->limitLabelWidth(cardW - ps(14.f), 0.44f, 0.18f);
            cardNode->addChild(desc2, 5);

            auto btn = CCMenuItemExt::createSpriteExtra(
                cardNode, [this, i](CCMenuItemSpriteExtra*) {
                    this->selectMode(i);
                }
            );
            btn->setPosition({lx + cardW * 0.5f, ly + cardH * 0.5f});
            gridMenu->addChild(btn);
        }

        // Espacio inferior reservado para futuras herramientas al deslizar hacia abajo
        auto futureBox = CCDrawNode::create();
        ui::drawRoundedRect(
            futureBox, ps(15.f), ps(12.f), viewW - ps(30.f), ps(58.f), ps(12.f),
            rgba4F(ccc3(0x03, 0x16, 0x42), 0.75f), ps(1.8f), rgba4F(COL_DIVIDER, 0.85f)
        );
        content->addChild(futureBox, 2);

        auto futureLbl = CCLabelBMFont::create(
            "Proximamente: mas modos y herramientas inteligentes de DecoMind AI",
            "chatFont.fnt"
        );
        futureLbl->setColor(COL_TEXT_SOFT);
        futureLbl->setScale(0.46f);
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
    // 4. PAGINA 1 (MODOS): FICHA DE USUARIO DEL MODO
    // -----------------------------------------------------------------------
    void buildPageModosDetail() {
        auto page = m_centerPages[1];

        ui::addRefRoundedBox(
            page, 336.f, 192.f, 726.f, 624.f, 16.f,
            ccc3(0x02, 0x13, 0x42), 0.94f,
            COL_FRAME_BLUE, 0.85f, 2.5f, 1
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

        auto prevSpr = ui::makeCustomMenuBtn("< Anterior", ps(160.f), ps(48.f), COL_BG_SIDE_IDLE, COL_FRAME_BLUE, 0.28f);
        auto prevBtn = CCMenuItemExt::createSpriteExtra(prevSpr, [this](CCMenuItemSpriteExtra*) {
            int next = (m_selectedMode <= 0) ? 8 : (m_selectedMode - 1);
            this->selectMode(next);
        });
        prevBtn->setPosition({px(444.f), py(730.f)});
        menu->addChild(prevBtn);

        auto nextSpr = ui::makeCustomMenuBtn("Siguiente >", ps(160.f), ps(48.f), COL_BG_SIDE_IDLE, COL_FRAME_BLUE, 0.28f);
        auto nextBtn = CCMenuItemExt::createSpriteExtra(nextSpr, [this](CCMenuItemSpriteExtra*) {
            int next = (m_selectedMode < 0) ? 0 : ((m_selectedMode + 1) % 9);
            this->selectMode(next);
        });
        nextBtn->setPosition({px(698.f), py(730.f)});
        menu->addChild(nextBtn);

        auto useExSpr = ui::makeCustomMenuBtn("Cargar ejemplo en Chat", ps(270.f), ps(48.f), ccc3(0x06, 0x38, 0x44), COL_NEON_GREEN, 0.28f);
        auto useExBtn = CCMenuItemExt::createSpriteExtra(useExSpr, [this](CCMenuItemSpriteExtra*) {
            int idx = (m_selectedMode >= 0) ? m_selectedMode : 2;
            this->selectMode(idx);
            this->setPromptText(MODES[idx].examplePrompt);
            this->showPage(2);
        });
        useExBtn->setPosition({px(571.f), py(660.f)});
        menu->addChild(useExBtn);

        auto gridSpr = ui::makeCustomMenuBtn("Ver cuadricula", ps(186.f), ps(48.f), COL_BG_SIDE_IDLE, COL_FRAME_CYAN, 0.28f);
        auto gridBtn = CCMenuItemExt::createSpriteExtra(gridSpr, [this](CCMenuItemSpriteExtra*) {
            this->showPage(0);
        });
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

        if (m_modePageGuide) {
            m_modePageGuide->setText(m.userGuide);
        }
        if (m_modePageExample) {
            fitBMFontLabel(m_modePageExample, std::string("\"") + m.examplePrompt + "\"", ps(660.f), 0.52f);
        }
    }

    // -----------------------------------------------------------------------
    // 5. PAGINA 2 (CHAT): EL CUADRO CHAT / COMANDO SE MUEVE AL CENTRO (Imagen 6)
    // -----------------------------------------------------------------------
    void buildPageChatCenterContext() {
        auto page = m_centerPages[2];

        // Recuadro inferior de estado/respuesta debajo del cuadro CHAT / COMANDO trasladado al centro
        ui::addRefRoundedBox(
            page, 344.f, 648.f, 710.f, 168.f, 14.f,
            ccc3(0x02, 0x12, 0x3a), 0.94f,
            COL_FRAME_BLUE, 0.85f, 2.2f, 1
        );

        m_chatReplyStatusLbl = CCLabelBMFont::create("ESTADO DE LA IA: LISTO", "bigFont.fnt");
        m_chatReplyStatusLbl->setAnchorPoint({0.f, 0.5f});
        m_chatReplyStatusLbl->setColor(COL_NEON_GREEN);
        m_chatReplyStatusLbl->setPosition({px(368.f), py(678.f)});
        m_chatReplyStatusLbl->limitLabelWidth(ps(660.f), 0.30f, 0.16f);
        page->addChild(m_chatReplyStatusLbl, 5);

        m_chatReplyBodyLbl = CCLabelBMFont::create("", "chatFont.fnt");
        m_chatReplyBodyLbl->setAnchorPoint({0.f, 0.5f});
        m_chatReplyBodyLbl->setColor(COL_TEXT_WHITE);
        m_chatReplyBodyLbl->setPosition({px(368.f), py(736.f)});
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
            COL_FRAME_BLUE, 0.85f, 2.5f, 1
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
                COL_BG_RIGHT_CARD, 0.92f, COL_DIVIDER, 0.85f, 1.8f, 2
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
    // 7. PAGINA 4 (AJUSTES): REDISENO LIMPIO + PALETA Y CONTROL RGB (Imagenes 3 y 7)
    // -----------------------------------------------------------------------
    void buildPageAjustes() {
        auto page = m_centerPages[4];

        ui::addRefRoundedBox(
            page, 332.f, 184.f, 734.f, 640.f, 16.f,
            ccc3(0x02, 0x13, 0x42), 0.94f,
            COL_FRAME_BLUE, 0.85f, 2.5f, 1
        );

        auto hdr = CCLabelBMFont::create("AJUSTES Y PERSONALIZACION DEL MENU", "bigFont.fnt");
        hdr->setAnchorPoint({0.f, 0.5f});
        hdr->setColor(COL_TEXT_CYAN);
        hdr->setPosition({px(358.f), py(222.f)});
        hdr->limitLabelWidth(ps(680.f), 0.34f, 0.18f);
        page->addChild(hdr, 5);

        // 5 tarjetas de fila perfectamente alineadas y espaciadas
        const float rowTops[5] = {252.f, 342.f, 452.f, 540.f, 632.f};
        const float rowHeights[5] = {76.f, 96.f, 74.f, 78.f, 118.f};
        for (int r = 0; r < 5; ++r) {
            ui::addRefRoundedBox(
                page, 352.f, rowTops[r], 694.f, rowHeights[r], 10.f,
                COL_BG_RIGHT_CARD, 0.92f, COL_DIVIDER, 0.88f, 1.8f, 2
            );
        }

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        page->addChild(menu, 10);

        // --- FILA 1: Origen de generacion (y=252..328) ---
        auto lOrigin = CCLabelBMFont::create("Origen de IA:", "chatFont.fnt");
        lOrigin->setAnchorPoint({0.f, 0.5f});
        lOrigin->setColor(COL_TEXT_SOFT);
        lOrigin->setScale(0.52f);
        lOrigin->setPosition({px(372.f), py(276.f)});
        page->addChild(lOrigin, 5);

        m_originValueLbl = CCLabelBMFont::create("", "bigFont.fnt");
        m_originValueLbl->setAnchorPoint({0.f, 0.5f});
        m_originValueLbl->setPosition({px(372.f), py(306.f)});
        page->addChild(m_originValueLbl, 5);

        auto toggleSpr = ui::makeCustomMenuBtn("CAMBIAR ORIGEN", ps(196.f), ps(46.f), COL_BG_SIDE_IDLE, COL_FRAME_CYAN, 0.28f);
        auto toggleBtn = CCMenuItemExt::createSpriteExtra(toggleSpr, [this](CCMenuItemSpriteExtra*) {
            std::string cur = AIClient::getOriginMode();
            std::string next = (cur == "local") ? "server" : "local";
            AIClient::setOriginMode(next);
            this->refreshAjustesLabels();
            this->refreshUsagePanel();
            this->refreshStatsPage();
            this->setAjustesStatus(next == "local" ? "Origen cambiado a Local (prueba)" : "Origen cambiado a Servidor HTTP");
        });
        toggleBtn->setPosition({px(924.f), py(290.f)});
        menu->addChild(toggleBtn);

        // --- FILA 2: URL del servidor (y=342..438) ---
        auto lUrl = CCLabelBMFont::create("URL del servidor Python / proveedor IA:", "chatFont.fnt");
        lUrl->setAnchorPoint({0.f, 0.5f});
        lUrl->setColor(COL_TEXT_SOFT);
        lUrl->setScale(0.50f);
        lUrl->setPosition({px(372.f), py(364.f)});
        page->addChild(lUrl, 5);

        m_serverUrlInput = TextInput::create(ps(456.f), "http://127.0.0.1:8765/generate", "chatFont.fnt");
        m_serverUrlInput->setFilter("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/.:-_");
        m_serverUrlInput->setMaxCharCount(96);
        m_serverUrlInput->setString(AIClient::getServerUrl());
        m_serverUrlInput->setPosition({px(604.f), py(406.f)});
        page->addChild(m_serverUrlInput, 12);

        auto saveUrlSpr = ui::makeCustomMenuBtn("GUARDAR URL", ps(168.f), ps(44.f), ccc3(0x06, 0x38, 0x44), COL_NEON_GREEN, 0.26f);
        auto saveUrlBtn = CCMenuItemExt::createSpriteExtra(saveUrlSpr, [this](CCMenuItemSpriteExtra*) {
            std::string val = trimStr(std::string(m_serverUrlInput->getString().c_str()));
            if (val.empty()) {
                this->setAjustesStatus("Error: la URL no puede estar vacia.");
                return;
            }
            AIClient::setServerUrl(val);
            this->setAjustesStatus("URL del servidor guardada correctamente.");
        });
        saveUrlBtn->setPosition({px(938.f), py(406.f)});
        menu->addChild(saveUrlBtn);

        // --- FILA 3: Secciones en origen local (y=452..526) ---
        auto lSec = CCLabelBMFont::create("Secciones a generar (modo Local):", "chatFont.fnt");
        lSec->setAnchorPoint({0.f, 0.5f});
        lSec->setColor(COL_TEXT_WHITE);
        lSec->setScale(0.54f);
        lSec->setPosition({px(372.f), py(489.f)});
        page->addChild(lSec, 5);

        auto minusSpr = ui::makeCustomMenuBtn("-", ps(56.f), ps(44.f), COL_BG_SIDE_IDLE, COL_FRAME_BLUE, 0.40f);
        auto minusBtn = CCMenuItemExt::createSpriteExtra(minusSpr, [this](CCMenuItemSpriteExtra*) {
            AIClient::setLocalSections(AIClient::getLocalSections() - 1);
            this->refreshAjustesLabels();
        });
        minusBtn->setPosition({px(836.f), py(489.f)});
        menu->addChild(minusBtn);

        m_sectionsValueLbl = CCLabelBMFont::create("", "bigFont.fnt");
        m_sectionsValueLbl->setPosition({px(915.f), py(489.f)});
        m_sectionsValueLbl->setScale(0.40f);
        page->addChild(m_sectionsValueLbl, 5);

        auto plusSpr = ui::makeCustomMenuBtn("+", ps(56.f), ps(44.f), COL_BG_SIDE_IDLE, COL_FRAME_BLUE, 0.40f);
        auto plusBtn = CCMenuItemExt::createSpriteExtra(plusSpr, [this](CCMenuItemSpriteExtra*) {
            AIClient::setLocalSections(AIClient::getLocalSections() + 1);
            this->refreshAjustesLabels();
        });
        plusBtn->setPosition({px(994.f), py(489.f)});
        menu->addChild(plusBtn);

        // --- FILA 4: Paleta de colores del menu interactivo (y=540..618) ---
        auto lPal = CCLabelBMFont::create("Paleta de colores del menu interactivo:", "chatFont.fnt");
        lPal->setAnchorPoint({0.f, 0.5f});
        lPal->setColor(COL_TEXT_WHITE);
        lPal->setScale(0.54f);
        lPal->setPosition({px(372.f), py(579.f)});
        page->addChild(lPal, 5);

        m_themeSwatchNode = CCDrawNode::create();
        m_themeSwatchNode->setPosition({px(766.f), py(592.f)});
        page->addChild(m_themeSwatchNode, 6);

        auto pickThemeSpr = ui::makeCustomMenuBtn("CAMBIAR PALETA", ps(196.f), ps(44.f), COL_BG_SIDE_IDLE, COL_FRAME_CYAN, 0.26f);
        auto pickThemeBtn = CCMenuItemExt::createSpriteExtra(pickThemeSpr, [this](CCMenuItemSpriteExtra*) {
            if (auto pop = GDColorPickerPopup::create(
                "PALETA DEL MENU INTERACTIVO",
                ThemeConfig::getMenuAccent(),
                COL_FRAME_BLUE,
                [this](ccColor3B chosen) {
                    ThemeConfig::setMenuAccent(chosen);
                    this->refreshThemeVisuals();
                    this->setAjustesStatus("Paleta de colores del menu actualizada.");
                }
            )) {
                pop->show();
            }
        });
        pickThemeBtn->setPosition({px(924.f), py(579.f)});
        menu->addChild(pickThemeBtn);

        // --- FILA 5: Velocidad, estado y color de los recuadros RGB (y=632..750) ---
        auto lRgbTitle = CCLabelBMFont::create("Animacion y velocidad de bordes RGB:", "chatFont.fnt");
        lRgbTitle->setAnchorPoint({0.f, 0.5f});
        lRgbTitle->setColor(COL_TEXT_WHITE);
        lRgbTitle->setScale(0.52f);
        lRgbTitle->setPosition({px(372.f), py(662.f)});
        page->addChild(lRgbTitle, 5);

        m_rgbStateValueLbl = CCLabelBMFont::create("", "bigFont.fnt");
        m_rgbStateValueLbl->setAnchorPoint({0.f, 0.5f});
        m_rgbStateValueLbl->setPosition({px(372.f), py(716.f)});
        page->addChild(m_rgbStateValueLbl, 5);

        auto rgbToggleSpr = ui::makeCustomMenuBtn("ON / NORMAL", ps(128.f), ps(40.f), COL_BG_SIDE_IDLE, COL_FRAME_CYAN, 0.24f);
        auto rgbToggleBtn = CCMenuItemExt::createSpriteExtra(rgbToggleSpr, [this](CCMenuItemSpriteExtra*) {
            ThemeConfig::setRgbEnabled(!ThemeConfig::isRgbEnabled());
            this->refreshAjustesLabels();
            this->setAjustesStatus(
                ThemeConfig::isRgbEnabled() ? "Efecto RGB dinamico activado." : "Efecto RGB desactivado (color normal fijo)."
            );
        });
        rgbToggleBtn->setPosition({px(555.f), py(716.f)});
        menu->addChild(rgbToggleBtn);

        auto spdMinusSpr = ui::makeCustomMenuBtn("-", ps(42.f), ps(40.f), COL_BG_SIDE_IDLE, COL_FRAME_BLUE, 0.36f);
        auto spdMinusBtn = CCMenuItemExt::createSpriteExtra(spdMinusSpr, [this](CCMenuItemSpriteExtra*) {
            ThemeConfig::setRgbSpeedPct(ThemeConfig::getRgbSpeedPct() - 25);
            this->refreshAjustesLabels();
        });
        spdMinusBtn->setPosition({px(668.f), py(716.f)});
        menu->addChild(spdMinusBtn);

        m_rgbSpeedValueLbl = CCLabelBMFont::create("", "bigFont.fnt");
        m_rgbSpeedValueLbl->setPosition({px(744.f), py(716.f)});
        m_rgbSpeedValueLbl->setScale(0.30f);
        page->addChild(m_rgbSpeedValueLbl, 5);

        auto spdPlusSpr = ui::makeCustomMenuBtn("+", ps(42.f), ps(40.f), COL_BG_SIDE_IDLE, COL_FRAME_BLUE, 0.36f);
        auto spdPlusBtn = CCMenuItemExt::createSpriteExtra(spdPlusSpr, [this](CCMenuItemSpriteExtra*) {
            ThemeConfig::setRgbSpeedPct(ThemeConfig::getRgbSpeedPct() + 25);
            this->refreshAjustesLabels();
        });
        spdPlusBtn->setPosition({px(820.f), py(716.f)});
        menu->addChild(spdPlusBtn);

        m_rgbSwatchNode = CCDrawNode::create();
        m_rgbSwatchNode->setPosition({px(854.f), py(674.f)});
        page->addChild(m_rgbSwatchNode, 6);

        auto rgbColSpr = ui::makeCustomMenuBtn("COLOR RGB", ps(136.f), ps(40.f), COL_BG_SIDE_IDLE, ccc3(0xb6, 0x48, 0xf6), 0.24f);
        auto rgbColBtn = CCMenuItemExt::createSpriteExtra(rgbColSpr, [this](CCMenuItemSpriteExtra*) {
            if (auto pop = GDColorPickerPopup::create(
                "COLOR DE RECUADROS RGB",
                ThemeConfig::getCustomRgbColor(),
                COL_FRAME_CYAN,
                [this](ccColor3B chosen) {
                    ThemeConfig::setCustomRgbColor(chosen);
                    bool isDefault = (chosen.r == COL_FRAME_CYAN.r && chosen.g == COL_FRAME_CYAN.g && chosen.b == COL_FRAME_CYAN.b);
                    ThemeConfig::setUseCustomRgbBase(!isDefault);
                    this->refreshThemeVisuals();
                    this->setAjustesStatus(isDefault ? "Colores por paleta original restaurados." : "Color personalizado aplicado a los bordes RGB.");
                }
            )) {
                pop->show();
            }
        });
        rgbColBtn->setPosition({px(954.f), py(716.f)});
        menu->addChild(rgbColBtn);

        m_settingsStatusLbl = CCLabelBMFont::create(
            "Estado: listo. Personaliza los colores o conecta Server/server.py.",
            "chatFont.fnt"
        );
        m_settingsStatusLbl->setAnchorPoint({0.f, 0.5f});
        m_settingsStatusLbl->setColor(COL_TEXT_CYAN);
        m_settingsStatusLbl->setPosition({px(364.f), py(788.f)});
        fitBMFontLabel(
            m_settingsStatusLbl,
            "Estado: listo. Personaliza los colores o conecta Server/server.py.",
            ps(670.f), 0.50f
        );
        page->addChild(m_settingsStatusLbl, 5);

        this->refreshAjustesLabels();
    }

    void refreshAjustesLabels() {
        if (m_originValueLbl) {
            std::string origin = AIClient::getOriginMode();
            bool isLocal = (origin == "local");
            fitBMFontLabel(
                m_originValueLbl,
                isLocal ? "LOCAL (Prueba sin red)" : "SERVIDOR HTTP (Proveedor IA)",
                ps(450.f), 0.32f
            );
            m_originValueLbl->setColor(isLocal ? ccc3(0xfb, 0xcf, 0x50) : COL_NEON_GREEN);
        }
        if (m_sectionsValueLbl) {
            m_sectionsValueLbl->setString(fmt::format("{}", AIClient::getLocalSections()).c_str());
        }
        if (m_rgbStateValueLbl) {
            bool en = ThemeConfig::isRgbEnabled();
            fitBMFontLabel(m_rgbStateValueLbl, en ? "ACTIVO" : "NORMAL", ps(110.f), 0.30f);
            m_rgbStateValueLbl->setColor(en ? COL_NEON_GREEN : COL_TEXT_SOFT);
        }
        if (m_rgbSpeedValueLbl) {
            m_rgbSpeedValueLbl->setString(fmt::format("{}%", ThemeConfig::getRgbSpeedPct()).c_str());
        }
    }

    void setAjustesStatus(std::string const& s) {
        if (m_settingsStatusLbl) {
            fitBMFontLabel(m_settingsStatusLbl, s, ps(670.f), 0.50f);
        }
    }

    // -----------------------------------------------------------------------
    // 8. COLUMNA DERECHA: USO DE LA IA, CHAT / COMANDO (Movil) e HISTORIAL (Movil)
    // -----------------------------------------------------------------------
    void buildRightColumn() {
        // -------------------------------------------------------------------
        // PANEL 1: USO DE LA IA (x=1106..1502, y_top=142..406)
        // -------------------------------------------------------------------
        m_usoPanelBg = ui::addRefRoundedBox(
            m_mainLayer,
            1106.f, 142.f, 396.f, 264.f, 18.f,
            COL_BG_RIGHT_CARD, 0.96f,
            COL_FRAME_BLUE, 0.95f, 2.8f, 3
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
        // PANEL 2: CHAT / COMANDO (Contenedor movil `m_chatPanelNode`)
        // -------------------------------------------------------------------
        m_chatPanelNode = CCNode::create();
        m_chatPanelNode->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_chatPanelNode, 20);

        m_chatPanelBg = ui::addRefRoundedBox(
            m_chatPanelNode,
            1106.f, 420.f, 396.f, 268.f, 18.f,
            COL_BG_RIGHT_CARD, 0.96f,
            COL_FRAME_BLUE, 0.95f, 2.8f, 1
        );

        auto chatDn = CCDrawNode::create();
        ui::drawRoundedRect(
            chatDn, px(1124.f), py(456.f), ps(28.f), ps(20.f), ps(6.f),
            rgba4F(COL_TEXT_CYAN, 1.f), 0.f, ccc4f(0, 0, 0, 0)
        );
        for (int d = -1; d <= 1; ++d) {
            chatDn->drawDot(ccp(px(1138.f + d * 6.f), py(446.f)), ps(2.0f), rgba4F(COL_BG_RIGHT_CARD, 1.f));
        }
        chatDn->drawSegment(ccp(px(1122.f), py(472.f)), ccp(px(1486.f), py(472.f)), ps(1.5f), divCol);
        m_chatPanelNode->addChild(chatDn, 2);

        auto chatTitle = CCLabelBMFont::create("CHAT / COMANDO", "bigFont.fnt");
        chatTitle->setAnchorPoint({0.f, 0.5f});
        chatTitle->setColor(COL_TEXT_CYAN);
        chatTitle->setPosition({px(1164.f), py(446.f)});
        chatTitle->limitLabelWidth(ps(310.f), 0.34f, 0.18f);
        m_chatPanelNode->addChild(chatTitle, 3);

        ui::addRefRoundedBox(
            m_chatPanelNode,
            1126.f, 482.f, 358.f, 136.f, 14.f,
            COL_BG_INPUT, 0.98f,
            ccc3(0x16, 0x3b, 0x82), 0.95f, 2.2f, 2
        );

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
            pl->setPosition({px(1144.f), py(508.f + static_cast<float>(l) * 28.f)});
            fitBMFontLabel(pl, phLines[l], ps(324.f), 0.45f);
            m_placeholderNode->addChild(pl);
        }

        for (int l = 0; l < 3; ++l) {
            m_typedPreviewLines[l] = CCLabelBMFont::create("", "chatFont.fnt");
            m_typedPreviewLines[l]->setAnchorPoint({0.f, 0.5f});
            m_typedPreviewLines[l]->setColor(COL_TEXT_WHITE);
            m_typedPreviewLines[l]->setPosition({px(1144.f), py(536.f + static_cast<float>(l) * 28.f)});
            m_chatPanelNode->addChild(m_typedPreviewLines[l], 5);
        }

        m_promptInput = TextInput::create(ps(336.f), " ", "chatFont.fnt");
        m_promptInput->setCommonFilter(CommonFilter::Any);
        m_promptInput->setMaxCharCount(160);
        m_promptInput->setPosition({px(1305.f), py(510.f)});
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
            ps(2.4f),
            rgba4F(ccc3(0x3a, 0x78, 0xd8), 1.f)
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
            sendDn->drawSegment(
                ccp(sendW * 0.26f, sendH * 0.50f),
                ccp(sendW * 0.52f, sendH * 0.50f),
                ps(2.2f),
                rgba4F(ccc3(0x10, 0x32, 0x77), 1.f)
            );
        }
        sendNode->addChild(sendDn);

        auto sendBtn = CCMenuItemExt::createSpriteExtra(
            sendNode, [this](CCMenuItemSpriteExtra*) { this->onSendCommand(); }
        );
        sendBtn->setPosition({px(1451.f), py(650.f)});
        chatMenu->addChild(sendBtn);

        // -------------------------------------------------------------------
        // PANEL 3: HISTORIAL (Contenedor movil `m_histPanelNode`)
        // Se abre UNICAMENTE presionando el boton '>' (Imagen 1)
        // -------------------------------------------------------------------
        m_histPanelNode = CCNode::create();
        m_histPanelNode->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_histPanelNode, 18);

        m_histPanelBg = ui::addRefRoundedBox(
            m_histPanelNode,
            1106.f, 702.f, 396.f, 178.f, 18.f,
            COL_BG_RIGHT_CARD, 0.96f,
            COL_FRAME_BLUE, 0.95f, 2.8f, 1
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
            histDn->drawSegment(
                ccp(px(1138.f), py(728.f)), ccp(px(1138.f), py(720.f)),
                ps(2.2f), rgba4F(COL_BG_RIGHT_CARD, 1.f)
            );
            histDn->drawSegment(
                ccp(px(1138.f), py(728.f)), ccp(px(1145.f), py(728.f)),
                ps(2.2f), rgba4F(COL_BG_RIGHT_CARD, 1.f)
            );
        }

        histDn->drawSegment(ccp(px(1122.f), py(752.f)), ccp(px(1486.f), py(752.f)), ps(1.5f), divCol);
        histDn->drawSegment(ccp(px(1122.f), py(792.f)), ccp(px(1486.f), py(792.f)), ps(1.2f), divCol);
        histDn->drawSegment(ccp(px(1122.f), py(832.f)), ccp(px(1486.f), py(832.f)), ps(1.2f), divCol);

        const float rowY[3] = {772.f, 812.f, 852.f};
        for (int r = 0; r < 3; ++r) {
            histDn->drawDot(ccp(px(1135.f), py(rowY[r])), ps(4.5f), rgba4F(ccc3(0x3a, 0x86, 0xe8), 1.f));
        }
        m_histPanelNode->addChild(histDn, 2);

        auto histTitle = CCLabelBMFont::create("HISTORIAL", "bigFont.fnt");
        histTitle->setAnchorPoint({0.f, 0.5f});
        histTitle->setColor(COL_TEXT_CYAN);
        histTitle->setPosition({px(1164.f), py(728.f)});
        histTitle->limitLabelWidth(ps(240.f), 0.34f, 0.18f);
        m_histPanelNode->addChild(histTitle, 3);

        auto histMenu = CCMenu::create();
        histMenu->setPosition({0.f, 0.f});
        m_histPanelNode->addChild(histMenu, 10);

        // Botones '>' dedicados en la derecha para abrir la ventana de historial
        for (int r = 0; r < 3; ++r) {
            m_histRowText[r] = CCLabelBMFont::create("", "chatFont.fnt");
            m_histRowText[r]->setAnchorPoint({0.f, 0.5f});
            m_histRowText[r]->setColor(COL_TEXT_SOFT);
            m_histRowText[r]->setPosition({px(1152.f), py(rowY[r])});
            m_histPanelNode->addChild(m_histRowText[r], 4);

            m_histRowTime[r] = CCLabelBMFont::create("", "chatFont.fnt");
            m_histRowTime[r]->setAnchorPoint({1.f, 0.5f});
            m_histRowTime[r]->setColor(COL_TEXT_DIM);
            m_histRowTime[r]->setPosition({px(1450.f), py(rowY[r])});
            m_histPanelNode->addChild(m_histRowTime[r], 4);

            // Boton '>' exclusivo para abrir la ventana de historial (Imagen 1)
            auto arrowNode = ui::makeCustomMenuBtn(">", ps(32.f), ps(28.f), COL_BG_SIDE_IDLE, COL_FRAME_CYAN, 0.28f);
            auto arrowBtn = CCMenuItemExt::createSpriteExtra(
                arrowNode, [this](CCMenuItemSpriteExtra*) { this->openFullHistoryModal(); }
            );
            arrowBtn->setPosition({px(1472.f), py(rowY[r])});
            histMenu->addChild(arrowBtn);
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
    // 9. BARRA INFERIOR Y BANNER DEL ALIADO (Contorno RGB + Robot animado + Fade)
    // -----------------------------------------------------------------------
    void buildBottomBar() {
        ui::addRefRoundedBox(
            m_mainLayer,
            28.f, 898.f, 928.f, 78.f, 12.f,
            COL_BG_BOTTOM, 0.96f,
            COL_FRAME_BLUE, 0.90f, 2.2f, 3
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
            btn->setPosition({px(btnCentersX[i]), py(937.f)});
            m_fixedMenu->addChild(btn);
        }

        // Banner inferior derecho del Aliado Robot con contorno RGB (x=1090..1404, y_top=898..972)
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
    // 10. INDICADORES, ENVIO DE COMANDO Y BUCLE DE ANIMACION RGB / FADE
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

        float topFrac = std::min(1.f, std::max(0.12f, static_cast<float>(limit - used) / static_cast<float>(limit)));
        if (m_topBarFill) {
            m_topBarFill->clear();
            ui::drawRoundedRect(
                m_topBarFill,
                px(1126.f), py(67.f), ps(270.f * topFrac), ps(11.f), ps(5.f),
                rgba4F(ccc3(0x28, 0xe8, 0xc8), 1.f), 0.f, ccc4f(0, 0, 0, 0)
            );
        }
    }

    void refreshHistoryMiniPanel() {
        auto const& h = HistoryManager::entries();
        for (int r = 0; r < 3; ++r) {
            int k = static_cast<int>(h.size()) - 1 - r;
            if (k >= 0) {
                auto const& entry = h[k];
                fitBMFontLabel(m_histRowText[r], entry.summary, ps(196.f), 0.44f);
                std::string tStr = HistoryManager::formatRelativeTime(entry.timestamp);
                fitBMFontLabel(m_histRowTime[r], tStr, ps(85.f), 0.42f);
            } else {
                fitBMFontLabel(
                    m_histRowText[r],
                    r == 0 ? "Sin peticiones recientes" : "-",
                    ps(196.f), 0.44f
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

    // Bucle de animacion para contornos RGB, mascota robot y aparicion/desaparicion gradual de frases
    void tick(float dt) {
        m_animTime += dt;
        float rgbSpeed = ThemeConfig::getRgbSpeed();
        m_rgbPhase += dt * 2.6f * rgbSpeed;

        // 1. Mascota superior y cabeza del robot en el banner inferior animadas
        if (m_mascotTop) {
            m_mascotTop->setPositionY(std::sin(m_animTime * 2.4f) * 1.6f);
        }
        if (m_allyRobotNode) {
            m_allyRobotNode->setPosition({
                px(1142.f),
                py(935.f) + std::sin(m_animTime * 3.6f) * 1.8f
            });
            m_allyRobotNode->setScale(1.0f + 0.05f * std::sin(m_animTime * 4.5f));
        }

        // 2. Contornos RGB por paleta en las 9 tarjetas, picos inferiores y banner del aliado
        bool rgbOn = ThemeConfig::isRgbEnabled();
        bool useCustomBase = ThemeConfig::useCustomRgbBase();
        ccColor3B customBase = ThemeConfig::getCustomRgbColor();

        const float cardW = ps(230.f);
        const float cardH = ps(198.f);
        for (int i = 0; i < 9; ++i) {
            if (!m_cardBorderNodes[i]) continue;
            ccColor3B basePalette = useCustomBase ? customBase : MODES[i].borderColor;
            ccColor3B curBorder = rgbOn
                ? shiftPaletteColor(basePalette, m_rgbPhase + i * 0.55f)
                : basePalette;
            m_cardBorderNodes[i]->clear();
            ui::drawRoundedRect(
                m_cardBorderNodes[i], 0.f, 0.f, cardW, cardH, ps(15.f),
                ccc4f(0, 0, 0, 0), ps(3.2f), rgba4F(curBorder, 1.f)
            );
        }

        // Picos de Geometry Dash abajo a la izquierda con decoracion RGB
        ccColor3B spikeBase1 = useCustomBase ? customBase : COL_FRAME_CYAN;
        ccColor3B spikeBase2 = useCustomBase ? lerpColor(customBase, COL_FRAME_BLUE, 0.5f) : COL_FRAME_BLUE;
        ccColor3B spkCol1 = rgbOn ? hsvToRgbColor( std::fmod(m_rgbPhase * 0.12f, 1.f), 0.85f, 1.0f ) : spikeBase1;
        ccColor3B spkCol2 = rgbOn ? hsvToRgbColor( std::fmod(m_rgbPhase * 0.12f + 0.15f, 1.f), 0.90f, 0.85f ) : spikeBase2;
        ui::redrawSpikesRgb(m_spikesRgbNode, spkCol1, spkCol2);

        // Contorno RGB del banner inferior derecho del robot aliado
        if (m_allyBorderNode) {
            m_allyBorderNode->clear();
            ccColor3B allyBorderCol = rgbOn
                ? hsvToRgbColor(std::fmod(m_rgbPhase * 0.14f + 0.35f, 1.f), 0.82f, 1.0f)
                : (useCustomBase ? customBase : ccc3(0x18, 0x82, 0xf2));
            ui::drawRoundedRect(
                m_allyBorderNode,
                px(1090.f), py(972.f), ps(314.f), ps(74.f), ps(16.f),
                rgba4F(COL_BG_ALLY, 0.96f),
                ps(2.8f),
                rgba4F(allyBorderCol, 1.f)
            );
        }

        // 3. Aparicion y desaparicion gradual (fade-in / fade-out) de las frases del robot (Imagen 7)
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
            fadeFactor = m_phraseTimer / 1.0f; // Fade-in gradual (0 -> 1s)
        } else if (m_phraseTimer > (cycleDuration - 1.0f)) {
            fadeFactor = std::max(0.f, (cycleDuration - m_phraseTimer) / 1.0f); // Fade-out gradual
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
// Integracion en el Editor de Geometry Dash: Nuevo Boton Personalizado
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
