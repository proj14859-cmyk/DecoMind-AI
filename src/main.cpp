// ===========================================================================
// DecoMind AI v0.3 - Menu Interactivo Nativo para el Editor de Geometry Dash
//
// Arquitectura en capas (ver headers en src/):
//   - Modes.hpp          : Definicion de los 9 modos (ID interno, descripcion
//                          visible al usuario e instrucciones internas para IA).
//   - HistoryManager.hpp : Registro persistente de historial, favoritos y frases.
//   - LevelValidator.hpp : Validacion de objetos y parametros propuestos por IA.
//   - LevelExecutor.hpp  : Inspeccion del nivel, respaldos y creacion segura.
//   - AIClient.hpp       : Comunicacion desacoplada con servidor HTTP / local.
//   - UIRenderer.hpp     : Renderizado nativo Cocos2d-x fiel 1:1 a la referencia.
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
// Clase base para ventanas secundarias (Historial, Favoritos, Tutoriales, Niveles)
// con el mismo estilo visual sci-fi azul/cian del menu principal.
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

        auto dn = CCDrawNode::create();
        ui::drawRoundedRect(
            dn, 0.f, 0.f, w, h, 8.f,
            rgba4F(COL_BG_OUTER, 0.98f),
            2.2f,
            rgba4F(COL_FRAME_CYAN, 0.95f)
        );
        ui::drawRoundedRect(
            dn, 4.f, 4.f, w - 8.f, h - 8.f, 6.f,
            rgba4F(COL_BG_FRAME, 0.98f),
            1.2f,
            rgba4F(COL_FRAME_BLUE, 0.9f)
        );
        dn->drawSegment(ccp(12.f, h - 32.f), ccp(w - 12.f, h - 32.f), 1.f, rgba4F(COL_FRAME_BLUE, 0.8f));
        m_mainLayer->addChild(dn, 1);

        m_ui = CCMenu::create();
        m_ui->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_ui, 50);

        // Boton verde estilo referencia para cerrar la ventana secundaria
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
        auto node = CCNode::create();
        node->setContentSize({w, h});
        auto dn = CCDrawNode::create();
        ui::drawRoundedRect(
            dn, 0.f, 0.f, w, h, 5.f,
            rgba4F(fillCol, 0.95f),
            1.5f,
            rgba4F(borderCol, 1.f)
        );
        node->addChild(dn, 1);
        auto lbl = CCLabelBMFont::create(sanitizeForGD(text).c_str(), "bigFont.fnt");
        lbl->setColor(COL_TEXT_WHITE);
        lbl->setPosition({w * 0.5f, h * 0.5f});
        lbl->limitLabelWidth(w - 12.f, 0.34f, 0.16f);
        node->addChild(lbl, 2);
        return node;
    }
};

// ===========================================================================
// Ventana Secundaria 1: Historial Completo
// ===========================================================================
class HistoryPopup : public DMModalBase {
protected:
    std::function<void(int, std::string const&)> m_onPick;
    std::function<void()> m_onClear;
    ScrollLayer* m_scroll = nullptr;

    bool init(
        std::function<void(int, std::string const&)> onPick,
        std::function<void()> onClear
    ) {
        if (!this->initModal(340.f, 240.f, "HISTORIAL DE PETICIONES")) return false;
        m_onPick = std::move(onPick);
        m_onClear = std::move(onClear);

        auto boxDn = CCDrawNode::create();
        ui::drawRoundedRect(
            boxDn, 12.f, 44.f, 316.f, 156.f, 6.f,
            rgba4F(COL_BG_CENTER, 0.95f), 1.2f, rgba4F(COL_FRAME_BLUE, 0.8f)
        );
        m_mainLayer->addChild(boxDn, 2);

        m_scroll = ScrollLayer::create(CCSize{304.f, 146.f});
        m_scroll->ignoreAnchorPointForPosition(false);
        m_scroll->setPosition({170.f, 122.f});
        m_mainLayer->addChild(m_scroll, 5);

        auto clearSpr = this->makeStyledActionBtn("Borrar historial", 132.f, 24.f, ccc3(0x3a, 0x10, 0x28), ccc3(0xf7, 0x54, 0x75));
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
        float total = 6.f + static_cast<float>(h.size()) * (itemH + gap);
        total = std::max(viewH, total);
        layer->setContentSize({viewW, total});

        int row = 0;
        for (int k = static_cast<int>(h.size()) - 1; k >= 0; --k, ++row) {
            auto const& e = h[k];
            int mi = modeIndexById(e.modeId);
            ccColor3B accent = (mi >= 0) ? MODES[mi].borderColor : COL_FRAME_CYAN;
            std::string modeName = (mi >= 0) ? MODES[mi].name : "General";
            std::string relTime = HistoryManager::formatRelativeTime(e.timestamp);

            auto node = CCNode::create();
            node->setContentSize({294.f, itemH});
            auto dn = CCDrawNode::create();
            ui::drawRoundedRect(
                dn, 0.f, 0.f, 294.f, itemH, 5.f,
                rgba4F(COL_BG_RIGHT_CARD, 0.96f), 1.2f, rgba4F(accent, 0.85f)
            );
            dn->drawDot(ccp(12.f, itemH * 0.5f), 3.2f, rgba4F(accent, 1.f));
            node->addChild(dn, 1);

            auto lMode = CCLabelBMFont::create("", "bigFont.fnt");
            lMode->setAnchorPoint({0.f, 0.5f});
            lMode->setColor(accent);
            lMode->setPosition({22.f, 26.f});
            fitBMFontLabel(lMode, modeName, 160.f, 0.30f);
            node->addChild(lMode, 2);

            auto lTime = CCLabelBMFont::create("", "chatFont.fnt");
            lTime->setAnchorPoint({1.f, 0.5f});
            lTime->setColor(COL_TEXT_SOFT);
            lTime->setPosition({284.f, 26.f});
            fitBMFontLabel(lTime, relTime + " >", 95.f, 0.48f);
            node->addChild(lTime, 2);

            auto lPrompt = CCLabelBMFont::create("", "chatFont.fnt");
            lPrompt->setAnchorPoint({0.f, 0.5f});
            lPrompt->setColor(COL_TEXT_WHITE);
            lPrompt->setPosition({22.f, 11.f});
            fitBMFontLabel(lPrompt, e.prompt, 262.f, 0.52f);
            node->addChild(lPrompt, 2);

            std::string promptCopy = e.prompt;
            auto menu = CCMenu::create();
            menu->setContentSize({viewW, itemH});
            menu->setPosition({0.f, total - 4.f - static_cast<float>(row) * (itemH + gap) - itemH});
            auto btn = CCMenuItemExt::createSpriteExtra(node, [this, mi, promptCopy](CCMenuItemSpriteExtra*) {
                if (m_onPick) m_onPick(mi, promptCopy);
                this->onClose(nullptr);
            });
            btn->setPosition({viewW * 0.5f, itemH * 0.5f});
            menu->addChild(btn);
            layer->addChild(menu);
        }
        m_scroll->scrollToTop();
    }

public:
    static HistoryPopup* create(
        std::function<void(int, std::string const&)> onPick,
        std::function<void()> onClear
    ) {
        auto ret = new HistoryPopup();
        if (ret->init(std::move(onPick), std::move(onClear))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ===========================================================================
// Ventana Secundaria 2: Favoritos (guardar y reutilizar peticiones frecuentes)
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
            rgba4F(COL_BG_CENTER, 0.95f), 1.2f, rgba4F(COL_FRAME_BLUE, 0.8f)
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
// Ventana Secundaria 3: Tutoriales de Uso y Guia del Servidor
// ===========================================================================
class TutorialsPopup : public DMModalBase {
protected:
    bool init() {
        if (!this->initModal(350.f, 240.f, "TUTORIALES Y GUIA RAPIDA")) return false;

        auto boxDn = CCDrawNode::create();
        ui::drawRoundedRect(
            boxDn, 12.f, 16.f, 326.f, 184.f, 6.f,
            rgba4F(COL_BG_CENTER, 0.95f), 1.2f, rgba4F(COL_FRAME_BLUE, 0.8f)
        );
        m_mainLayer->addChild(boxDn, 2);

        auto scroll = ScrollLayer::create(CCSize{310.f, 172.f});
        scroll->ignoreAnchorPointForPosition(false);
        scroll->setPosition({175.f, 108.f});
        m_mainLayer->addChild(scroll, 5);

        std::string guideText =
            "1. COMO USAR DECOMIND AI EN EL EDITOR\n"
            "- En la pestana INICIO, elige uno de los 9 modos haciendo clic en su tarjeta.\n"
            "- Escribe tu instruccion en lenguaje natural dentro del panel derecho CHAT / COMANDO.\n"
            "- Pulsa el boton de envio (avion de papel) para procesar la solicitud.\n\n"
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
            "3. CONEXION CON EL SERVIDOR (TERMUX / PC)\n"
            "- En la pestana AJUSTES (o icono de engranaje superior) puedes alternar entre:\n"
            "  * Origen Local (prueba sin red que coloca objetos de demostracion).\n"
            "  * Origen Servidor (conecta con Server/server.py en http://127.0.0.1:8765/generate).\n"
            "- Antes de aplicar cualquier cambio grande, usa el boton GUARDAR de la barra inferior.";

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
            rgba4F(COL_BG_CENTER, 0.95f), 1.2f, rgba4F(COL_FRAME_BLUE, 0.8f)
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
                rgba4F(COL_BG_RIGHT_CARD, 0.96f), 1.f, rgba4F(COL_FRAME_BLUE, 0.85f)
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
// Menu Principal de DecoMind AI (fiel 1:1 a la imagen de referencia)
// ===========================================================================
struct ChatMessage {
    bool isUser = false;
    std::string text;
};

class DecoMindPopup : public Popup {
protected:
    Ref<LevelEditorLayer> m_lel;
    AIClient m_aiClient;

    // Estado de la sesion y seleccion
    // En la referencia, "Modo activo" inicia en "Ninguno" (-1) hasta elegir una tarjeta
    int m_selectedMode = -1;
    int m_activePage = 0; // 0=INICIO, 1=MODOS, 2=CHAT, 3=ESTADISTICAS, 4=AJUSTES
    int m_pendingChatIdx = -1;
    int m_sessionRequests = 0;
    int m_sessionPlacedObjects = 0;
    std::vector<ChatMessage> m_chatLog;

    // Menus interactivos principales
    CCMenu* m_fixedMenu = nullptr;
    CCMenu* m_sidebarMenu = nullptr;

    // Contenedores de las 5 vistas centrales (dentro de x=308..1090, y_top=163..844)
    CCNode* m_centerPages[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    CCMenuItemSpriteExtra* m_sidebarBtns[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};

    // Header superior
    CCNode* m_mascotTop = nullptr;
    CCDrawNode* m_topBarFill = nullptr;
    float m_animTime = 0.f;

    // Pagina 0 (INICIO - Cuadricula 3x3 de modos)
    CCDrawNode* m_cardSelectionGlow[9] = {nullptr};

    // Pagina 1 (MODOS - Ficha de usuario del modo seleccionado, sin preprompt interno)
    CCLabelBMFont* m_modePageTitle = nullptr;
    CCLabelBMFont* m_modePageSubtitle = nullptr;
    SimpleTextArea* m_modePageGuide = nullptr;
    CCLabelBMFont* m_modePageExample = nullptr;

    // Pagina 2 (CHAT - Historial de conversacion y respuestas)
    ScrollLayer* m_chatScroll = nullptr;
    SimpleTextArea* m_chatArea = nullptr;
    CCLabelBMFont* m_chatStateLbl = nullptr;

    // Pagina 3 (ESTADISTICAS)
    CCLabelBMFont* m_statsLabels[7] = {nullptr};

    // Pagina 4 (AJUSTES)
    CCLabelBMFont* m_originValueLbl = nullptr;
    TextInput* m_serverUrlInput = nullptr;
    CCLabelBMFont* m_sectionsValueLbl = nullptr;
    CCLabelBMFont* m_settingsStatusLbl = nullptr;

    // Columna Derecha - Panel 1: USO DE LA IA
    CCDrawNode* m_quotaBarFill = nullptr;
    CCLabelBMFont* m_quotaNumLbl = nullptr;
    CCLabelBMFont* m_quotaMaxLbl = nullptr;
    CCLabelBMFont* m_reqDoneValLbl = nullptr;
    CCLabelBMFont* m_curLevelValLbl = nullptr;
    CCLabelBMFont* m_activeModeValLbl = nullptr;

    // Columna Derecha - Panel 2: CHAT / COMANDO
    TextInput* m_promptInput = nullptr;
    CCNode* m_placeholderNode = nullptr;
    CCLabelBMFont* m_typedPreviewLines[3] = {nullptr, nullptr, nullptr};

    // Columna Derecha - Panel 3: HISTORIAL (3 filas interactivas)
    CCLabelBMFont* m_histRowText[3] = {nullptr, nullptr, nullptr};
    CCLabelBMFont* m_histRowTime[3] = {nullptr, nullptr, nullptr};

    // Barra Inferior Derecha - Banner del Aliado Robot
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

        // Ajuste proporcional automatico al tamano de ventana de Geometry Dash
        auto win = CCDirector::get()->getWinSize();
        float scale = std::min(1.f, std::min((win.width - 8.f) / PW, (win.height - 6.f) / PH));
        m_mainLayer->setScale(scale);

        // 1. Marco exterior sci-fi y picos de Geometry Dash abajo a la izquierda
        ui::drawSciFiMainFrame(m_mainLayer);

        // 2. Contenedor central redondeado (x=308..1090, y_top=163..844)
        ui::addRefRoundedBox(
            m_mainLayer,
            308.f, 163.f, 782.f, 681.f, 22.f,
            COL_BG_CENTER, 0.96f,
            COL_FRAME_BLUE, 0.95f,
            3.0f, 2
        );

        // Menus de botones
        m_fixedMenu = CCMenu::create();
        m_fixedMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_fixedMenu, 60);

        m_sidebarMenu = CCMenu::create();
        m_sidebarMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_sidebarMenu, 55);

        // 3. Construccion de secciones fieles a la imagen de referencia
        this->buildTopHeader();
        this->buildSidebar();

        for (int i = 0; i < 5; ++i) {
            m_centerPages[i] = CCNode::create();
            m_mainLayer->addChild(m_centerPages[i], 10);
        }
        this->buildPageInicioGrid();
        this->buildPageModosDetail();
        this->buildPageChat();
        this->buildPageStats();
        this->buildPageAjustes();

        this->buildRightColumn();
        this->buildBottomBar();

        // Estado inicial sincronizado
        this->selectMode(-1); // "Ninguno" como en la referencia (o seleccionable con 1 clic)
        this->showPage(0);
        this->refreshUsagePanel();
        this->refreshHistoryMiniPanel();
        this->refreshStatsPage();
        this->refreshChatPage();
        this->updateAllyBannerText();

        this->schedule(schedule_selector(DecoMindPopup::tick));
        return true;
    }

    // -----------------------------------------------------------------------
    // 1. HEADER SUPERIOR: Flecha verde, Engranaje, Mascota Robot, Barra y Diamante
    // -----------------------------------------------------------------------
    void buildTopHeader() {
        // Boton Verde Volver/Cerrar (x=31..114, y_top=18..106 -> centro 72.5, 62)
        auto backNode = ui::createBackArrowIcon();
        auto backBtn = CCMenuItemExt::createSpriteExtra(
            backNode, [this](CCMenuItemSpriteExtra*) { this->onClose(nullptr); }
        );
        backBtn->setPosition({px(72.5f), py(62.f)});
        m_fixedMenu->addChild(backBtn);

        // Boton Engranaje Superior (x=172..255, y_top=24..102 -> centro 213.5, 63)
        auto gearNode = ui::createTopGearButtonNode();
        auto gearBtn = CCMenuItemExt::createSpriteExtra(
            gearNode, [this](CCMenuItemSpriteExtra*) { this->showPage(4); }
        );
        gearBtn->setPosition({px(213.5f), py(63.f)});
        m_fixedMenu->addChild(gearBtn);

        // Mascota Robot asomandose en el centro superior (x=828, y_top=80)
        m_mascotTop = ui::createRobotMascotNode(true);
        auto mascotBtn = CCMenuItemExt::createSpriteExtra(
            m_mascotTop, [this](CCMenuItemSpriteExtra*) {
                this->nextAllyPhrase();
            }
        );
        mascotBtn->setPosition({px(828.f), py(80.f)});
        m_fixedMenu->addChild(mascotBtn);

        // Capsula superior derecha con estrella y barra de progreso (x=1058..1416, y_top=34..88)
        ui::addRefRoundedBox(
            m_mainLayer,
            1058.f, 34.f, 358.f, 54.f, 14.f,
            ccc3(0x03, 0x13, 0x3c), 0.96f,
            ccc3(0x12, 0x4c, 0xaa), 0.95f,
            2.8f, 3
        );

        auto topCapsDn = CCDrawNode::create();
        ui::drawSparkle4(topCapsDn, ccp(px(1092.f), py(61.f)), ps(15.f), rgba4F(ccc3(0x38, 0xf2, 0xd8), 1.f));
        // Pista oscura de la barra superior (x=1126..1396, y_top=56..67)
        ui::drawRoundedRect(
            topCapsDn,
            px(1126.f), py(67.f), ps(270.f), ps(11.f), ps(5.f),
            rgba4F(ccc3(0x06, 0x1c, 0x46), 1.f), 0.f, ccc4f(0, 0, 0, 0)
        );
        m_mainLayer->addChild(topCapsDn, 4);

        m_topBarFill = CCDrawNode::create();
        m_mainLayer->addChild(m_topBarFill, 5);

        // Diamante superior derecho (x=1481, y_top=60)
        auto diamondNode = ui::createTopDiamondButtonNode();
        auto diamondBtn = CCMenuItemExt::createSpriteExtra(
            diamondNode, [this](CCMenuItemSpriteExtra*) { this->showPage(3); }
        );
        diamondBtn->setPosition({px(1481.f), py(60.f)});
        m_fixedMenu->addChild(diamondBtn);
    }

    // -----------------------------------------------------------------------
    // 2. BARRA LATERAL IZQUIERDA (5 Botones: INICIO, MODOS, CHAT, ESTADISTICAS, AJUSTES)
    // -----------------------------------------------------------------------
    void buildSidebar() {
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
            m_sidebarBtns[i] = btn;
        }
    }

    void showPage(int p) {
        if (p < 0 || p >= 5) p = 0;
        m_activePage = p;
        for (int i = 0; i < 5; ++i) {
            if (m_centerPages[i]) m_centerPages[i]->setVisible(i == p);
        }
        this->buildSidebar();

        if (m_serverUrlInput) {
            m_serverUrlInput->setEnabled(p == 4);
        }
        if (p == 1) this->refreshModeDetailPage();
        if (p == 2) this->refreshChatPage();
        if (p == 3) this->refreshStatsPage();
    }

    // -----------------------------------------------------------------------
    // 3. PAGINA 0 (INICIO): CUADRICULA 3x3 DE LOS 9 MODOS (Fiel a la referencia)
    // -----------------------------------------------------------------------
    void buildPageInicioGrid() {
        auto page = m_centerPages[0];
        auto gridMenu = CCMenu::create();
        gridMenu->setPosition({0.f, 0.f});
        page->addChild(gridMenu, 10);

        const float colX[3] = {333.f, 585.f, 837.f};
        const float rowY[3] = {186.f, 412.f, 638.f};
        const float cardW_ref = 230.f;
        const float cardH_ref = 198.f;
        const float w = ps(cardW_ref);
        const float h = ps(cardH_ref);

        for (int i = 0; i < 9; ++i) {
            int r = i / 3;
            int c = i % 3;
            float rx = colX[c];
            float ry = rowY[r];
            auto const& m = MODES[i];

            auto cardNode = CCNode::create();
            cardNode->setContentSize({w, h});

            // Fondo y borde neon propios del modo
            auto dn = CCDrawNode::create();
            ui::drawRoundedRect(
                dn, -0.8f, -0.8f, w + 1.6f, h + 1.6f, ps(16.f),
                ccc4f(0.f, 0.f, 0.f, 0.f),
                ps(4.5f),
                rgba4F(m.borderColor, 0.30f)
            );
            ui::drawRoundedRect(
                dn, 0.f, 0.f, w, h, ps(15.f),
                rgba4F(m.bgColor, 0.96f),
                ps(3.0f),
                rgba4F(m.borderColor, 1.f)
            );
            cardNode->addChild(dn, 1);

            // Capa de resaltado cuando la tarjeta esta seleccionada
            auto selGlow = CCDrawNode::create();
            ui::drawRoundedRect(
                selGlow, ps(3.f), ps(3.f), w - ps(6.f), h - ps(6.f), ps(12.f),
                rgba4F(m.borderColor, 0.16f),
                ps(2.2f),
                rgba4F(COL_TEXT_WHITE, 0.92f)
            );
            selGlow->setVisible(false);
            cardNode->addChild(selGlow, 2);
            m_cardSelectionGlow[i] = selGlow;

            // Icono centrado en la mitad superior de la tarjeta
            auto iconNode = ui::createModeIconNode(i);
            iconNode->ignoreAnchorPointForPosition(false);
            iconNode->setAnchorPoint({0.5f, 0.5f});
            iconNode->setPosition({w * 0.5f, h * 0.67f});
            cardNode->addChild(iconNode, 3);

            // Titulo en mayusculas blanco (ESTRUCTURAR, LAYOUT, DECO, etc.)
            auto titleLbl = CCLabelBMFont::create(m.title, "bigFont.fnt");
            titleLbl->setColor(COL_TEXT_WHITE);
            titleLbl->setPosition({w * 0.5f, h * 0.36f});
            titleLbl->limitLabelWidth(w - ps(16.f), 0.30f, 0.14f);
            cardNode->addChild(titleLbl, 4);

            // Descripcion visible de 2 lineas debajo del titulo
            auto desc1 = CCLabelBMFont::create(m.visibleLine1, "chatFont.fnt");
            desc1->setColor(ccc3(0xd8, 0xee, 0xff));
            desc1->setPosition({w * 0.5f, h * 0.21f});
            desc1->limitLabelWidth(w - ps(14.f), 0.44f, 0.18f);
            cardNode->addChild(desc1, 4);

            auto desc2 = CCLabelBMFont::create(m.visibleLine2, "chatFont.fnt");
            desc2->setColor(ccc3(0xd8, 0xee, 0xff));
            desc2->setPosition({w * 0.5f, h * 0.11f});
            desc2->limitLabelWidth(w - ps(14.f), 0.44f, 0.18f);
            cardNode->addChild(desc2, 4);

            auto btn = CCMenuItemExt::createSpriteExtra(
                cardNode, [this, i](CCMenuItemSpriteExtra*) {
                    if (m_selectedMode == i) {
                        // Segundo clic sobre el mismo modo abre su ficha o alterna
                        this->selectMode(i);
                    } else {
                        this->selectMode(i);
                    }
                }
            );
            btn->setPosition({px(rx + cardW_ref * 0.5f), py(ry + cardH_ref * 0.5f)});
            gridMenu->addChild(btn);
        }
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
    // 4. PAGINA 1 (MODOS): FICHA DE USUARIO DEL MODO (SIN PREPROMPT INTERNO)
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

        // Botones para cambiar entre los 9 modos, usar el ejemplo o volver a la cuadricula
        auto prevSpr = ButtonSprite::create("< Anterior", 86, true, "bigFont.fnt", "GJ_button_01.png", 26.f, 0.34f);
        auto prevBtn = CCMenuItemExt::createSpriteExtra(prevSpr, [this](CCMenuItemSpriteExtra*) {
            int next = (m_selectedMode <= 0) ? 8 : (m_selectedMode - 1);
            this->selectMode(next);
        });
        prevBtn->setPosition({px(430.f), py(730.f)});
        menu->addChild(prevBtn);

        auto nextSpr = ButtonSprite::create("Siguiente >", 86, true, "bigFont.fnt", "GJ_button_01.png", 26.f, 0.34f);
        auto nextBtn = CCMenuItemExt::createSpriteExtra(nextSpr, [this](CCMenuItemSpriteExtra*) {
            int next = (m_selectedMode < 0) ? 0 : ((m_selectedMode + 1) % 9);
            this->selectMode(next);
        });
        nextBtn->setPosition({px(715.f), py(730.f)});
        menu->addChild(nextBtn);

        auto useExSpr = ButtonSprite::create("Usar ejemplo en Chat", 142, true, "bigFont.fnt", "GJ_button_02.png", 26.f, 0.34f);
        auto useExBtn = CCMenuItemExt::createSpriteExtra(useExSpr, [this](CCMenuItemSpriteExtra*) {
            int idx = (m_selectedMode >= 0) ? m_selectedMode : 2;
            this->selectMode(idx);
            this->setPromptText(MODES[idx].examplePrompt);
        });
        useExBtn->setPosition({px(572.f), py(660.f)});
        menu->addChild(useExBtn);

        auto gridSpr = ButtonSprite::create("Ver cuadricula 3x3", 126, true, "bigFont.fnt", "GJ_button_04.png", 26.f, 0.34f);
        auto gridBtn = CCMenuItemExt::createSpriteExtra(gridSpr, [this](CCMenuItemSpriteExtra*) {
            this->showPage(0);
        });
        gridBtn->setPosition({px(945.f), py(730.f)});
        menu->addChild(gridBtn);
    }

    void refreshModeDetailPage() {
        if (!m_modePageTitle) return;
        int idx = (m_selectedMode >= 0 && m_selectedMode < 9) ? m_selectedMode : 0;
        auto const& m = MODES[idx];

        std::string headerName = (m_selectedMode >= 0)
            ? std::string(m.title)
            : std::string(m.title) + " (Pulsa Seleccionar)";
        fitBMFontLabel(m_modePageTitle, headerName, ps(660.f), 0.48f);
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
    // 5. PAGINA 2 (CHAT): CONVERSACION, RESULTADOS Y ESTADOS DE CARGA
    // -----------------------------------------------------------------------
    void buildPageChat() {
        auto page = m_centerPages[2];

        ui::addRefRoundedBox(
            page, 336.f, 192.f, 726.f, 624.f, 16.f,
            ccc3(0x02, 0x13, 0x42), 0.94f,
            COL_FRAME_BLUE, 0.85f, 2.5f, 1
        );

        auto hdr = CCLabelBMFont::create("CONVERSACION Y RESULTADOS DE LA IA", "bigFont.fnt");
        hdr->setAnchorPoint({0.f, 0.5f});
        hdr->setColor(COL_TEXT_CYAN);
        hdr->setPosition({px(364.f), py(228.f)});
        hdr->limitLabelWidth(ps(480.f), 0.34f, 0.18f);
        page->addChild(hdr, 5);

        m_chatStateLbl = CCLabelBMFont::create("Listo", "chatFont.fnt");
        m_chatStateLbl->setAnchorPoint({1.f, 0.5f});
        m_chatStateLbl->setColor(COL_NEON_GREEN);
        m_chatStateLbl->setPosition({px(1036.f), py(228.f)});
        m_chatStateLbl->setScale(0.52f);
        page->addChild(m_chatStateLbl, 5);

        float scW = ps(678.f);
        float scH = ps(480.f);
        m_chatScroll = ScrollLayer::create(CCSize{scW, scH});
        m_chatScroll->ignoreAnchorPointForPosition(false);
        m_chatScroll->setPosition({px(699.f), py(505.f)});
        page->addChild(m_chatScroll, 6);

        auto holder = CCNode::create();
        holder->setContentSize({scW, scH});
        m_chatScroll->m_contentLayer->addChild(holder);

        m_chatArea = SimpleTextArea::create(" ", "chatFont.fnt", 0.52f, scW - 14.f);
        m_chatArea->setWrappingMode(WORD_WRAP);
        m_chatArea->setColor(rgba4B(COL_TEXT_WHITE, 255));
        m_chatArea->setAnchorPoint({0.f, 1.f});
        holder->addChild(m_chatArea);

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        page->addChild(menu, 10);

        auto cancelSpr = ButtonSprite::create("Cancelar envio", 108, true, "bigFont.fnt", "GJ_button_06.png", 24.f, 0.32f);
        auto cancelBtn = CCMenuItemExt::createSpriteExtra(cancelSpr, [this](CCMenuItemSpriteExtra*) {
            if (m_aiClient.isBusy()) {
                m_aiClient.cancel();
                this->resolvePendingChat("Operacion cancelada por el usuario.");
                if (m_chatStateLbl) {
                    m_chatStateLbl->setString("Cancelado");
                    m_chatStateLbl->setColor(ccc3(0xfb, 0xcf, 0x50));
                }
            }
        });
        cancelBtn->setPosition({px(540.f), py(778.f)});
        menu->addChild(cancelBtn);

        auto clearSpr = ButtonSprite::create("Limpiar chat", 104, true, "bigFont.fnt", "GJ_button_04.png", 24.f, 0.32f);
        auto clearBtn = CCMenuItemExt::createSpriteExtra(clearSpr, [this](CCMenuItemSpriteExtra*) {
            m_chatLog.clear();
            m_pendingChatIdx = -1;
            this->refreshChatPage();
        });
        clearBtn->setPosition({px(860.f), py(778.f)});
        menu->addChild(clearBtn);
    }

    void appendChat(bool isUser, std::string const& text) {
        m_chatLog.push_back({isUser, sanitizeForGD(text)});
        if (m_chatLog.size() > 50) {
            m_chatLog.erase(m_chatLog.begin());
            if (m_pendingChatIdx >= 0) --m_pendingChatIdx;
        }
        this->refreshChatPage();
    }

    void resolvePendingChat(std::string const& replyText) {
        std::string clean = sanitizeForGD(replyText);
        if (m_pendingChatIdx >= 0 && m_pendingChatIdx < static_cast<int>(m_chatLog.size())) {
            m_chatLog[m_pendingChatIdx].text = clean;
            m_pendingChatIdx = -1;
            this->refreshChatPage();
        } else {
            this->appendChat(false, clean);
        }
    }

    void refreshChatPage() {
        if (!m_chatArea || !m_chatScroll) return;

        std::string out;
        if (m_chatLog.empty()) {
            out = "Aun no hay mensajes en esta sesion.\n\n"
                  "1. Selecciona uno de los 9 modos en la cuadricula de INICIO.\n"
                  "2. Escribe lo que quieres hacer en el panel derecho CHAT / COMANDO.\n"
                  "3. Pulsa el boton de enviar (avion de papel) para ver el resultado aqui.";
        } else {
            for (int i = static_cast<int>(m_chatLog.size()) - 1; i >= 0; --i) {
                out += (m_chatLog[i].isUser ? "[TU]: " : "[DECOMIND AI]: ") + m_chatLog[i].text + "\n\n";
            }
        }

        m_chatArea->setText(out);
        float pad = 6.f;
        float viewH = m_chatScroll->getContentHeight();
        float w = m_chatScroll->getContentWidth();
        float h = std::max(viewH, m_chatArea->getHeight() + pad * 2.f);
        m_chatScroll->m_contentLayer->setContentSize({w, h});
        m_chatArea->getParent()->setContentSize({w, h});
        m_chatArea->setPosition({pad, h - pad});
        m_chatScroll->scrollToTop();
    }

    // -----------------------------------------------------------------------
    // 6. PAGINA 3 (ESTADISTICAS): CONSUMO Y METRICAS REALES
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
            "Origen configurado: " + std::string(origin == "local" ? "Local (modo de prueba sin red)" : "Servidor HTTP (IA)"),
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
    // 7. PAGINA 4 (AJUSTES): CONFIGURACION DE SERVIDOR, SECCIONES Y FRASES
    // -----------------------------------------------------------------------
    void buildPageAjustes() {
        auto page = m_centerPages[4];

        ui::addRefRoundedBox(
            page, 336.f, 192.f, 726.f, 624.f, 16.f,
            ccc3(0x02, 0x13, 0x42), 0.94f,
            COL_FRAME_BLUE, 0.85f, 2.5f, 1
        );

        auto hdr = CCLabelBMFont::create("AJUSTES DE CONEXION Y PREFERENCIAS", "bigFont.fnt");
        hdr->setAnchorPoint({0.f, 0.5f});
        hdr->setColor(COL_TEXT_CYAN);
        hdr->setPosition({px(364.f), py(236.f)});
        hdr->limitLabelWidth(ps(660.f), 0.36f, 0.18f);
        page->addChild(hdr, 5);

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        page->addChild(menu, 10);

        // 1. Selector de origen (Local vs Servidor)
        auto lOrigin = CCLabelBMFont::create("Origen de generacion:", "chatFont.fnt");
        lOrigin->setAnchorPoint({0.f, 0.5f});
        lOrigin->setColor(COL_TEXT_WHITE);
        lOrigin->setScale(0.58f);
        lOrigin->setPosition({px(364.f), py(308.f)});
        page->addChild(lOrigin, 5);

        m_originValueLbl = CCLabelBMFont::create("", "bigFont.fnt");
        m_originValueLbl->setAnchorPoint({0.f, 0.5f});
        m_originValueLbl->setPosition({px(364.f), py(348.f)});
        page->addChild(m_originValueLbl, 5);

        auto toggleSpr = ButtonSprite::create("Cambiar origen", 120, true, "bigFont.fnt", "GJ_button_01.png", 26.f, 0.34f);
        auto toggleBtn = CCMenuItemExt::createSpriteExtra(toggleSpr, [this](CCMenuItemSpriteExtra*) {
            std::string cur = AIClient::getOriginMode();
            std::string next = (cur == "local") ? "server" : "local";
            AIClient::setOriginMode(next);
            this->refreshAjustesLabels();
            this->refreshUsagePanel();
            this->refreshStatsPage();
            this->setAjustesStatus(next == "local" ? "Origen cambiado a Local (prueba)" : "Origen cambiado a Servidor HTTP");
        });
        toggleBtn->setPosition({px(910.f), py(328.f)});
        menu->addChild(toggleBtn);

        // 2. URL del servidor
        auto lUrl = CCLabelBMFont::create("URL del servidor Python / proveedor IA:", "chatFont.fnt");
        lUrl->setAnchorPoint({0.f, 0.5f});
        lUrl->setColor(COL_TEXT_WHITE);
        lUrl->setScale(0.58f);
        lUrl->setPosition({px(364.f), py(418.f)});
        page->addChild(lUrl, 5);

        m_serverUrlInput = TextInput::create(ps(500.f), "http://127.0.0.1:8765/generate", "chatFont.fnt");
        m_serverUrlInput->setFilter("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/.:-_");
        m_serverUrlInput->setMaxCharCount(96);
        m_serverUrlInput->setString(AIClient::getServerUrl());
        m_serverUrlInput->setPosition({px(618.f), py(472.f)});
        page->addChild(m_serverUrlInput, 12);

        auto saveUrlSpr = ButtonSprite::create("Guardar", 76, true, "bigFont.fnt", "GJ_button_02.png", 26.f, 0.34f);
        auto saveUrlBtn = CCMenuItemExt::createSpriteExtra(saveUrlSpr, [this](CCMenuItemSpriteExtra*) {
            std::string val = trimStr(std::string(m_serverUrlInput->getString().c_str()));
            if (val.empty()) {
                this->setAjustesStatus("Error: la URL no puede estar vacia.");
                return;
            }
            AIClient::setServerUrl(val);
            this->setAjustesStatus("URL del servidor guardada correctamente.");
        });
        saveUrlBtn->setPosition({px(955.f), py(472.f)});
        menu->addChild(saveUrlBtn);

        // 3. Secciones en origen local
        auto lSec = CCLabelBMFont::create("Secciones a generar (modo Local de prueba):", "chatFont.fnt");
        lSec->setAnchorPoint({0.f, 0.5f});
        lSec->setColor(COL_TEXT_WHITE);
        lSec->setScale(0.58f);
        lSec->setPosition({px(364.f), py(556.f)});
        page->addChild(lSec, 5);

        auto minusSpr = ButtonSprite::create("-", 30, true, "bigFont.fnt", "GJ_button_01.png", 26.f, 0.45f);
        auto minusBtn = CCMenuItemExt::createSpriteExtra(minusSpr, [this](CCMenuItemSpriteExtra*) {
            AIClient::setLocalSections(AIClient::getLocalSections() - 1);
            this->refreshAjustesLabels();
        });
        minusBtn->setPosition({px(830.f), py(556.f)});
        menu->addChild(minusBtn);

        m_sectionsValueLbl = CCLabelBMFont::create("", "bigFont.fnt");
        m_sectionsValueLbl->setPosition({px(905.f), py(556.f)});
        m_sectionsValueLbl->setScale(0.45f);
        page->addChild(m_sectionsValueLbl, 5);

        auto plusSpr = ButtonSprite::create("+", 30, true, "bigFont.fnt", "GJ_button_01.png", 26.f, 0.45f);
        auto plusBtn = CCMenuItemExt::createSpriteExtra(plusSpr, [this](CCMenuItemSpriteExtra*) {
            AIClient::setLocalSections(AIClient::getLocalSections() + 1);
            this->refreshAjustesLabels();
        });
        plusBtn->setPosition({px(980.f), py(556.f)});
        menu->addChild(plusBtn);

        // 4. Recarga de frases del aliado
        auto reloadSpr = ButtonSprite::create("Recargar frases.txt", 136, true, "bigFont.fnt", "GJ_button_04.png", 26.f, 0.34f);
        auto reloadBtn = CCMenuItemExt::createSpriteExtra(reloadSpr, [this](CCMenuItemSpriteExtra*) {
            m_phrases = HistoryManager::loadPhrases();
            m_phraseIdx = 0;
            this->updateAllyBannerText();
            this->setAjustesStatus(fmt::format("Se cargaron {} frases del aliado.", m_phrases.size()));
        });
        reloadBtn->setPosition({px(485.f), py(648.f)});
        menu->addChild(reloadBtn);

        m_settingsStatusLbl = CCLabelBMFont::create(
            "Estado: listo. Configura Server/server.py para conectar un proveedor IA.",
            "chatFont.fnt"
        );
        m_settingsStatusLbl->setAnchorPoint({0.f, 0.5f});
        m_settingsStatusLbl->setColor(COL_TEXT_CYAN);
        m_settingsStatusLbl->setPosition({px(364.f), py(736.f)});
        fitBMFontLabel(
            m_settingsStatusLbl,
            "Estado: listo. Configura Server/server.py para conectar un proveedor IA.",
            ps(660.f), 0.52f
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
                isLocal ? "LOCAL (Demostracion sin red)" : "SERVIDOR HTTP (Proveedor IA)",
                ps(460.f), 0.36f
            );
            m_originValueLbl->setColor(isLocal ? ccc3(0xfb, 0xcf, 0x50) : COL_NEON_GREEN);
        }
        if (m_sectionsValueLbl) {
            m_sectionsValueLbl->setString(fmt::format("{}", AIClient::getLocalSections()).c_str());
        }
    }

    void setAjustesStatus(std::string const& s) {
        if (m_settingsStatusLbl) {
            fitBMFontLabel(m_settingsStatusLbl, s, ps(660.f), 0.52f);
        }
    }

    // -----------------------------------------------------------------------
    // 8. COLUMNA DERECHA: 3 PANELES (USO DE LA IA, CHAT / COMANDO, HISTORIAL)
    // -----------------------------------------------------------------------
    void buildRightColumn() {
        // -------------------------------------------------------------------
        // PANEL 1: USO DE LA IA (x=1106..1502, y_top=142..406)
        // -------------------------------------------------------------------
        ui::addRefRoundedBox(
            m_mainLayer,
            1106.f, 142.f, 396.f, 264.f, 18.f,
            COL_BG_RIGHT_CARD, 0.96f,
            COL_FRAME_BLUE, 0.95f, 2.8f, 3
        );

        auto usoDn = CCDrawNode::create();
        // Estrella cian en el encabezado USO DE LA IA
        ui::drawSparkle4(usoDn, ccp(px(1138.f), py(169.f)), ps(14.f), rgba4F(ccc3(0x52, 0xd8, 0xff), 1.f));
        // Lineas divisorias horizontales
        ccColor4F divCol = rgba4F(COL_DIVIDER, 0.85f);
        usoDn->drawSegment(ccp(px(1122.f), py(192.f)), ccp(px(1486.f), py(192.f)), ps(1.5f), divCol);
        usoDn->drawSegment(ccp(px(1122.f), py(304.f)), ccp(px(1486.f), py(304.f)), ps(1.5f), divCol);
        usoDn->drawSegment(ccp(px(1122.f), py(348.f)), ccp(px(1486.f), py(348.f)), ps(1.5f), divCol);

        // Icono '+' pequeno junto a Limite diario (x=1135, y_top=212)
        ccColor4F iconSoft = rgba4F(COL_TEXT_SOFT, 0.95f);
        usoDn->drawSegment(ccp(px(1130.f), py(212.f)), ccp(px(1140.f), py(212.f)), ps(1.8f), rgba4F(COL_BAR_TEAL, 1.f));
        usoDn->drawSegment(ccp(px(1135.f), py(207.f)), ccp(px(1135.f), py(217.f)), ps(1.8f), rgba4F(COL_BAR_TEAL, 1.f));

        // Pista oscura de la barra de progreso (x=1138..1464, y_top=235..255)
        ui::drawRoundedRect(
            usoDn,
            px(1138.f), py(255.f), ps(326.f), ps(20.f), ps(10.f),
            rgba4F(ccc3(0x09, 0x1c, 0x48), 1.f), 0.f, ccc4f(0, 0, 0, 0)
        );

        // Icono burbuja en "Peticiones realizadas" (x=1138, y_top=282)
        ui::drawRoundedRect(
            usoDn, px(1128.f), py(290.f), ps(20.f), ps(15.f), ps(4.f),
            iconSoft, 0.f, iconSoft
        );

        // Icono play en "Nivel actual" (x=1138, y_top=326)
        CCPoint playPts[3] = {
            ccp(px(1131.f), py(318.f)),
            ccp(px(1146.f), py(326.f)),
            ccp(px(1131.f), py(334.f))
        };
        usoDn->drawPolygon(playPts, 3, iconSoft, 0.f, iconSoft);

        // Icono pildora en "Modo activo" (x=1138, y_top=372)
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

        m_quotaNumLbl = CCLabelBMFont::create("12", "bigFont.fnt");
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
        // PANEL 2: CHAT / COMANDO (x=1106..1502, y_top=420..688)
        // -------------------------------------------------------------------
        ui::addRefRoundedBox(
            m_mainLayer,
            1106.f, 420.f, 396.f, 268.f, 18.f,
            COL_BG_RIGHT_CARD, 0.96f,
            COL_FRAME_BLUE, 0.95f, 2.8f, 3
        );

        auto chatDn = CCDrawNode::create();
        // Icono de burbuja con 3 puntos en CHAT / COMANDO (x=1138, y_top=446)
        ui::drawRoundedRect(
            chatDn, px(1124.f), py(456.f), ps(28.f), ps(20.f), ps(6.f),
            rgba4F(COL_TEXT_CYAN, 1.f), 0.f, ccc4f(0, 0, 0, 0)
        );
        for (int d = -1; d <= 1; ++d) {
            chatDn->drawDot(ccp(px(1138.f + d * 6.f), py(446.f)), ps(2.0f), rgba4F(COL_BG_RIGHT_CARD, 1.f));
        }
        chatDn->drawSegment(ccp(px(1122.f), py(472.f)), ccp(px(1486.f), py(472.f)), ps(1.5f), divCol);
        m_mainLayer->addChild(chatDn, 4);

        auto chatTitle = CCLabelBMFont::create("CHAT / COMANDO", "bigFont.fnt");
        chatTitle->setAnchorPoint({0.f, 0.5f});
        chatTitle->setColor(COL_TEXT_CYAN);
        chatTitle->setPosition({px(1164.f), py(446.f)});
        chatTitle->limitLabelWidth(ps(310.f), 0.34f, 0.18f);
        m_mainLayer->addChild(chatTitle, 6);

        // Caja de texto redondeada (x=1126..1484, y_top=482..618)
        ui::addRefRoundedBox(
            m_mainLayer,
            1126.f, 482.f, 358.f, 136.f, 14.f,
            COL_BG_INPUT, 0.98f,
            ccc3(0x16, 0x3b, 0x82), 0.95f, 2.2f, 4
        );

        // Placeholder multilinea identico a la imagen de referencia
        m_placeholderNode = CCNode::create();
        m_mainLayer->addChild(m_placeholderNode, 7);
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

        // Vista previa multilinea cuando el usuario escribe un comando largo
        for (int l = 0; l < 3; ++l) {
            m_typedPreviewLines[l] = CCLabelBMFont::create("", "chatFont.fnt");
            m_typedPreviewLines[l]->setAnchorPoint({0.f, 0.5f});
            m_typedPreviewLines[l]->setColor(COL_TEXT_WHITE);
            m_typedPreviewLines[l]->setPosition({px(1144.f), py(536.f + static_cast<float>(l) * 28.f)});
            m_mainLayer->addChild(m_typedPreviewLines[l], 8);
        }

        // TextInput interactivo de Geode colocado en la parte superior de la caja de comando
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
        m_mainLayer->addChild(m_promptInput, 25);

        // Boton de envio (x=1418..1484, y_top=628..672) con icono de avion de papel
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
        sendNode->addChild(sendDn);

        auto sendBtn = CCMenuItemExt::createSpriteExtra(
            sendNode, [this](CCMenuItemSpriteExtra*) { this->onSendCommand(); }
        );
        sendBtn->setPosition({px(1451.f), py(650.f)});
        m_fixedMenu->addChild(sendBtn);

        // -------------------------------------------------------------------
        // PANEL 3: HISTORIAL (x=1106..1502, y_top=702..880)
        // -------------------------------------------------------------------
        ui::addRefRoundedBox(
            m_mainLayer,
            1106.f, 702.f, 396.f, 178.f, 18.f,
            COL_BG_RIGHT_CARD, 0.96f,
            COL_FRAME_BLUE, 0.95f, 2.8f, 3
        );

        auto histDn = CCDrawNode::create();
        // Icono de reloj en HISTORIAL (x=1138, y_top=728)
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

        histDn->drawSegment(ccp(px(1122.f), py(752.f)), ccp(px(1486.f), py(752.f)), ps(1.5f), divCol);
        histDn->drawSegment(ccp(px(1122.f), py(792.f)), ccp(px(1486.f), py(792.f)), ps(1.2f), divCol);
        histDn->drawSegment(ccp(px(1122.f), py(832.f)), ccp(px(1486.f), py(832.f)), ps(1.2f), divCol);

        const float rowY[3] = {772.f, 812.f, 852.f};
        for (int r = 0; r < 3; ++r) {
            histDn->drawDot(ccp(px(1135.f), py(rowY[r])), ps(4.5f), rgba4F(ccc3(0x3a, 0x86, 0xe8), 1.f));
        }
        m_mainLayer->addChild(histDn, 4);

        // Titulo HISTORIAL pulsable para abrir la ventana modal completa
        auto histHdrNode = CCNode::create();
        histHdrNode->setContentSize({ps(320.f), ps(32.f)});
        auto histTitle = CCLabelBMFont::create("HISTORIAL", "bigFont.fnt");
        histTitle->setAnchorPoint({0.f, 0.5f});
        histTitle->setColor(COL_TEXT_CYAN);
        histTitle->setPosition({0.f, ps(16.f)});
        histTitle->limitLabelWidth(ps(280.f), 0.34f, 0.18f);
        histHdrNode->addChild(histTitle);

        auto histHdrBtn = CCMenuItemExt::createSpriteExtra(
            histHdrNode, [this](CCMenuItemSpriteExtra*) { this->openFullHistoryModal(); }
        );
        histHdrBtn->setPosition({px(1324.f), py(728.f)});
        m_fixedMenu->addChild(histHdrBtn);

        // 3 filas interactivas del historial
        for (int r = 0; r < 3; ++r) {
            m_histRowText[r] = CCLabelBMFont::create("", "chatFont.fnt");
            m_histRowText[r]->setAnchorPoint({0.f, 0.5f});
            m_histRowText[r]->setColor(COL_TEXT_SOFT);
            m_histRowText[r]->setPosition({px(1152.f), py(rowY[r])});
            m_mainLayer->addChild(m_histRowText[r], 6);

            m_histRowTime[r] = CCLabelBMFont::create("", "chatFont.fnt");
            m_histRowTime[r]->setAnchorPoint({1.f, 0.5f});
            m_histRowTime[r]->setColor(COL_TEXT_DIM);
            m_histRowTime[r]->setPosition({px(1486.f), py(rowY[r])});
            m_mainLayer->addChild(m_histRowTime[r], 6);

            auto hitZone = CCNode::create();
            hitZone->setContentSize({ps(364.f), ps(36.f)});
            auto rowBtn = CCMenuItemExt::createSpriteExtra(
                hitZone, [this, r](CCMenuItemSpriteExtra*) { this->onHistoryRowClicked(r); }
            );
            rowBtn->setPosition({px(1304.f), py(rowY[r])});
            m_fixedMenu->addChild(rowBtn);
        }
    }

    void onPromptTextChanged(std::string const& text) {
        std::string clean = trimStr(sanitizeForGD(text));
        if (m_placeholderNode) {
            m_placeholderNode->setVisible(clean.empty());
        }
        // Si el texto supera una linea, muestra las lineas adicionales debajo
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
    // 9. BARRA INFERIOR COMPLETA: 5 Botones + Banner del Aliado Robot
    // -----------------------------------------------------------------------
    void buildBottomBar() {
        // Barra izquierda con 5 compartimentos (x=28..956, y_top=898..976)
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

            auto iconDn = CCDrawNode::create();
            ui::drawBottomBarIcon(iconDn, i, ccp(bw * 0.5f, bh * 0.64f));
            itemNode->addChild(iconDn, 1);

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

        // Banner inferior derecho del Aliado Robot (x=1090..1404, y_top=898..972)
        ui::addRefRoundedBox(
            m_mainLayer,
            1090.f, 898.f, 314.f, 74.f, 16.f,
            COL_BG_ALLY, 0.96f,
            ccc3(0x18, 0x82, 0xf2), 0.98f, 2.8f, 3
        );

        auto allyDn = CCDrawNode::create();
        allyDn->drawSegment(
            ccp(px(1188.f), py(910.f)),
            ccp(px(1188.f), py(960.f)),
            ps(1.6f),
            rgba4F(COL_DIVIDER, 0.9f)
        );
        // Estrella cian a la derecha del banner (x=1418, y_top=935)
        ui::drawSparkle4(allyDn, ccp(px(1418.f), py(935.f)), ps(13.f), rgba4F(ccc3(0x38, 0xd8, 0xff), 1.f));
        m_mainLayer->addChild(allyDn, 4);

        auto allyRobot = ui::createRobotMascotNode(false);
        allyRobot->ignoreAnchorPointForPosition(false);
        allyRobot->setAnchorPoint({0.5f, 0.5f});
        allyRobot->setPosition({px(1142.f), py(935.f)});
        m_mainLayer->addChild(allyRobot, 5);

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

        auto allyHit = CCNode::create();
        allyHit->setContentSize({ps(314.f), ps(74.f)});
        auto allyBtn = CCMenuItemExt::createSpriteExtra(
            allyHit, [this](CCMenuItemSpriteExtra*) { this->nextAllyPhrase(); }
        );
        allyBtn->setPosition({px(1247.f), py(935.f)});
        m_fixedMenu->addChild(allyBtn);
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

        // Parte la frase en 2 lineas equilibradas para el banner
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
    // 10. ACTUALIZACION DE INDICADORES Y ACCIONES INTERACTIVAS
    // -----------------------------------------------------------------------
    void refreshUsagePanel() {
        int used = Mod::get()->getSavedValue<int>("used", 0);
        int limit = Mod::get()->getSavedValue<int>("limit", 20);
        int total = Mod::get()->getSavedValue<int>("total", 0);
        if (limit < 1) limit = 20;

        if (m_quotaNumLbl) {
            m_quotaNumLbl->setString(fmt::format("{}", used).c_str());
        }
        if (m_quotaMaxLbl) {
            m_quotaMaxLbl->setString(fmt::format("/ {}", limit).c_str());
        }
        if (m_reqDoneValLbl) {
            m_reqDoneValLbl->setString(fmt::format("{}", total).c_str());
        }
        if (m_curLevelValLbl) {
            fitBMFontLabel(m_curLevelValLbl, LevelExecutor::getLevelTitle(m_lel), ps(155.f), 0.30f);
        }

        float frac = std::min(1.f, std::max(0.04f, static_cast<float>(used) / static_cast<float>(limit)));
        if (used == 0) {
            // Muestra un pequeno indicador inicial visible
            frac = 0.06f;
        }
        if (m_quotaBarFill) {
            m_quotaBarFill->clear();
            ui::drawRoundedRect(
                m_quotaBarFill,
                px(1138.f), py(255.f), ps(326.f * frac), ps(20.f), ps(10.f),
                rgba4F(COL_BAR_TEAL, 1.f), 0.f, ccc4f(0, 0, 0, 0)
            );
        }

        // Actualiza tambien la barra superior derecha
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
                fitBMFontLabel(m_histRowText[r], entry.summary, ps(220.f), 0.44f);
                std::string tStr = HistoryManager::formatRelativeTime(entry.timestamp) + "  >";
                fitBMFontLabel(m_histRowTime[r], tStr, ps(105.f), 0.42f);
            } else {
                fitBMFontLabel(
                    m_histRowText[r],
                    r == 0 ? "Sin peticiones recientes" : "-",
                    ps(220.f), 0.44f
                );
                fitBMFontLabel(m_histRowTime[r], ">", ps(40.f), 0.42f);
            }
        }
    }

    void onHistoryRowClicked(int rowIdx) {
        auto const& h = HistoryManager::entries();
        int k = static_cast<int>(h.size()) - 1 - rowIdx;
        if (k >= 0 && k < static_cast<int>(h.size())) {
            int mi = modeIndexById(h[k].modeId);
            if (mi >= 0) this->selectMode(mi);
            this->setPromptText(h[k].prompt);
        } else {
            this->openFullHistoryModal();
        }
    }

    void openFullHistoryModal() {
        auto popup = HistoryPopup::create(
            [this](int mi, std::string const& prompt) {
                if (mi >= 0) this->selectMode(mi);
                this->setPromptText(prompt);
            },
            [this]() {
                HistoryManager::clearHistory();
                this->refreshHistoryMiniPanel();
            }
        );
        if (popup) popup->show();
    }

    void onBottomBarAction(int idx) {
        switch (idx) {
            case 0: { // FAVORITOS
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
            case 1: { // TUTORIALES
                if (auto pop = TutorialsPopup::create()) {
                    pop->show();
                }
                break;
            }
            case 2: { // MIS NIVELES
                if (auto pop = LevelsBackupPopup::create(m_lel)) {
                    pop->show();
                }
                break;
            }
            case 3: { // GUARDAR
                std::string msg;
                bool ok = LevelExecutor::saveLevelSnapshot(m_lel, msg);
                FLAlertLayer::create(
                    ok ? "Guardado Completado" : "Aviso",
                    msg,
                    "OK"
                )->show();
                break;
            }
            default: { // REINICIAR
                if (m_aiClient.isBusy()) {
                    m_aiClient.cancel();
                }
                this->setPromptText("");
                this->selectMode(-1);
                m_pendingChatIdx = -1;
                if (m_chatStateLbl) {
                    m_chatStateLbl->setString("Listo");
                    m_chatStateLbl->setColor(COL_NEON_GREEN);
                }
                this->showPage(0);
                break;
            }
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
            this->appendChat(
                false,
                "Escribe primero una instruccion en el cuadro CHAT / COMANDO (o elige un ejemplo en MODOS / FAVORITOS)."
            );
            return;
        }

        // Si el usuario aun no habia marcado un modo ("Ninguno"), seleccionamos Deco por defecto
        int modeToUse = (m_selectedMode >= 0 && m_selectedMode < 9) ? m_selectedMode : 2;
        if (m_selectedMode < 0) {
            this->selectMode(modeToUse);
        }

        this->setPromptText("");
        this->showPage(2);
        this->appendChat(true, fmt::format("[{}] {}", MODES[modeToUse].name, prompt));

        HistoryManager::addEntry(MODES[modeToUse].id, prompt);
        this->refreshHistoryMiniPanel();

        m_sessionRequests += 1;
        int total = Mod::get()->getSavedValue<int>("total", 0) + 1;
        Mod::get()->setSavedValue<int>("total", total);

        if (m_chatStateLbl) {
            m_chatStateLbl->setString("Procesando...");
            m_chatStateLbl->setColor(ccc3(0xfb, 0xcf, 0x50));
        }

        this->appendChat(false, "Procesando solicitud...");
        m_pendingChatIdx = static_cast<int>(m_chatLog.size()) - 1;

        m_aiClient.sendRequest(
            modeToUse,
            prompt,
            m_lel,
            [this, modeToUse](AIResponseData const& res) {
                if (!res.ok) {
                    if (m_chatStateLbl) {
                        m_chatStateLbl->setString("Revision requerida");
                        m_chatStateLbl->setColor(ccc3(0xf7, 0x54, 0x75));
                    }
                    this->resolvePendingChat(res.message);
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
                        finalMsg += fmt::format("\n[Validacion OK: {} objetos colocados en el nivel].", vr.validCount);
                    } else if (!vr.note.empty()) {
                        finalMsg += "\n[Validacion: " + vr.note + "]";
                    }
                }

                if (m_chatStateLbl) {
                    m_chatStateLbl->setString("Completado");
                    m_chatStateLbl->setColor(COL_NEON_GREEN);
                }
                this->resolvePendingChat(finalMsg);
                this->refreshUsagePanel();
                this->refreshStatsPage();
            }
        );

        this->refreshUsagePanel();
        this->refreshStatsPage();
    }

    void tick(float dt) {
        m_animTime += dt;
        if (m_mascotTop) {
            m_mascotTop->setPositionY(std::sin(m_animTime * 2.4f) * 1.4f);
        }

        m_phraseTimer += dt;
        if (m_phraseTimer > 8.5f) {
            this->nextAllyPhrase();
        }
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
// Integracion en el Editor de Geometry Dash (Boton "IA")
// ===========================================================================
class $modify(DecoMindEditorUI, EditorUI) {
    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;

        auto spr = ButtonSprite::create("IA", "bigFont.fnt", "GJ_button_02.png", 0.8f);
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(DecoMindEditorUI::onOpenDecoMind)
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
