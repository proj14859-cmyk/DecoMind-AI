// ===========================================================================
// DecoMind AI v0.3.8-alpha - Interfaz UI Completa Fiel al PDF Oficial
//
// Implementa las 6 paginas de DecoMind_AI_interfaz_pulida.pdf:
//   - Pagina 1 (INICIO): Cuadricula 3x3 deslizable con el dedo, tarjetas con
//     icono en recuadro, marca de seleccion y estela RGB; columna derecha con
//     USO DE LA IA, CHAT / COMANDO (con contador 0/160 y soporte de ñ) e HISTORIAL.
//   - Pagina 2 (INFORMACION / MODOS): Lista maestra izquierda de los 9 modos con
//     sus etiquetas (Orden, Jugable, Visual, Mixto, Bloques, Logica, Consejos) y
//     panel derecho con DETALLES DEL MODO, pildoras PUEDE USAR (✓ / ✕),
//     EJEMPLO DE PRUEBA y botones USAR ESTE MODO y PROBAR EL EJEMPLO.
//   - Pagina 3 (CHAT): Vista amplia de conversacion con selector de Modo, estado
//     LISTO, burbuja del usuario en color Acento, burbuja de respuesta con
//     pildoras de resumen, botones DESHACER y GUARDAR EN FAVORITOS, chips
//     rapidos (/catalogo, Usar seleccion, etc.) y columna derecha con LO QUE VE
//     LA IA (Nivel, Seleccionados, Zona visible, Catalogo) y GRUPOS Y COLORES
//     LIBRES (G12 / C5).
//   - Pagina 4 (ESTADISTICAS): 4 tarjetas KPI superiores, grafico de barras de
//     PETICIONES · ULTIMOS 7 DIAS (L M X J V S D), grafico horizontal de MODOS
//     MAS USADOS y tabla inferior de PUNTOS DE GUARDADO con boton RESTAURAR.
//   - Pagina 5 (AJUSTES): Columna deslizable con Modo de conexion (Modo local /
//     Modo AI - Servidor HTTP), Direccion del servidor, Color del menu (8 tonos
//     + Rueda de color), Flujo RGB en contornos (Velocidad + interruptor),
//     Color de bordes, pinchos y robot RGB, Boton del editor (Opacidad +
//     Restablecer posicion) y Fuente de letras ("cambia la fuente de letras de
//     la ui") con menu desplegable hacia arriba del mismo ancho de la caja y
//     pantalla de carga difuminada/oscurecida con circulo giratorio; mas panel
//     derecho integrado SELECTOR DE COLOR (Rueda oficial de GD).
//   - Pagina 6 (SISTEMA CROMATICO): Un color -> toda la paleta (13 tokens HSL).
// ===========================================================================
#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextArea.hpp>
#include <Geode/ui/TextInput.hpp>

#include "AIClient.hpp"
#include "EditorButton.hpp"
#include "HistoryManager.hpp"
#include "LevelExecutor.hpp"
#include "LevelValidator.hpp"
#include "Modes.hpp"
#include "UIRenderer.hpp"

using namespace geode::prelude;
using namespace decomind;

namespace decomind {

// ===========================================================================
// Clase base para ventanas secundarias (con soporte de deslizamiento tactil)
// ===========================================================================
class DMModalBase : public Popup {
protected:
    CCMenu* m_ui = nullptr;
    ScrollLayer* m_modalScroll = nullptr;
    float m_w = 0.f;
    float m_h = 0.f;
    bool m_scrollTouchActive = false;
    bool m_scrollDragging = false;
    CCPoint m_scrollTouchStart{0.f, 0.f};
    float m_scrollStartContentY = 0.f;

    bool initModal(float w, float h, char const* titleText, bool showHeaderLine = true) {
        if (!Popup::init(w, h)) return false;
        m_w = w;
        m_h = h;

        m_bgSprite->setVisible(false);
        m_closeBtn->setVisible(false);

        auto pal = ThemeConfig::getPalette();

        auto dn = CCDrawNode::create();
        ui::drawRoundedRect(
            dn, 0.f, 0.f, w, h, 10.f,
            rgba4F(pal.bgOuter, 0.98f),
            1.6f,
            rgba4F(pal.lineStrong, 0.92f)
        );
        ui::drawRoundedRect(
            dn, 3.f, 3.f, w - 6.f, h - 6.f, 8.f,
            rgba4F(pal.panel, 0.98f),
            1.0f,
            rgba4F(pal.line, 0.88f)
        );
        if (showHeaderLine) {
            dn->drawSegment(ccp(12.f, h - 32.f), ccp(w - 12.f, h - 32.f), 0.8f, rgba4F(pal.line, 0.85f));
        }
        m_mainLayer->addChild(dn, 1);

        m_ui = CCMenu::create();
        m_ui->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_ui, 50);

        if (titleText && titleText[0] != '\0') {
            auto backIcon = ui::createBackArrowIcon();
            backIcon->setScale(0.72f);
            auto closeBtn = CCMenuItemExt::createSpriteExtra(
                backIcon, [this](CCMenuItemSpriteExtra*) { this->onClose(nullptr); }
            );
            closeBtn->m_scaleMultiplier = 1.04f;
            closeBtn->setPosition({22.f, h - 17.f});
            m_ui->addChild(closeBtn);

            if (auto title = ui::createStyledLabel(titleText, true, 0.36f, pal.text, w - 80.f)) {
                title->setPosition({w * 0.5f, h - 17.f});
                m_mainLayer->addChild(title, 10);
            }
        }

        return true;
    }

    CCNode* makeStyledActionBtn(char const* text, float w, float h, ccColor3B fillCol, ccColor3B borderCol, bool darkInk = false) {
        return ui::makeCustomMenuBtn(text, w, h, fillCol, borderCol, 0.26f, darkInk, 5.f);
    }

    bool isTouchInsideModalScroll(CCTouch* t) {
        if (!t || !m_modalScroll || !m_modalScroll->isVisible()) return false;
        CCPoint local = m_modalScroll->convertToNodeSpace(t->getLocation());
        CCSize sz = m_modalScroll->getContentSize();
        return (local.x >= 0.f && local.x <= sz.width && local.y >= 0.f && local.y <= sz.height);
    }

    bool ccTouchBegan(CCTouch* t, CCEvent* e) override {
        if (this->isTouchInsideModalScroll(t) && m_modalScroll->m_contentLayer) {
            m_scrollTouchActive = true;
            m_scrollDragging = false;
            m_scrollTouchStart = t->getLocation();
            m_scrollStartContentY = m_modalScroll->m_contentLayer->getPositionY();
            return true;
        }
        return Popup::ccTouchBegan(t, e);
    }

    void ccTouchMoved(CCTouch* t, CCEvent* e) override {
        if (m_scrollTouchActive && t && m_modalScroll && m_modalScroll->m_contentLayer) {
            CCPoint d = t->getLocation() - m_scrollTouchStart;
            if (!m_scrollDragging && std::abs(d.y) > 4.f) {
                m_scrollDragging = true;
            }
            if (m_scrollDragging) {
                CCPoint curLocal = m_modalScroll->convertToNodeSpace(t->getLocation());
                CCPoint startLocal = m_modalScroll->convertToNodeSpace(m_scrollTouchStart);
                float dy = curLocal.y - startLocal.y;
                float viewH = m_modalScroll->getContentSize().height;
                float contentH = m_modalScroll->m_contentLayer->getContentSize().height;
                float minY = std::min(0.f, viewH - contentH);
                float maxY = 0.f;
                m_modalScroll->m_contentLayer->setPositionY(
                    std::clamp(m_scrollStartContentY + dy, minY, maxY)
                );
            }
            return;
        }
        Popup::ccTouchMoved(t, e);
    }

    void ccTouchEnded(CCTouch* t, CCEvent* e) override {
        if (m_scrollTouchActive) {
            m_scrollTouchActive = false;
            m_scrollDragging = false;
            return;
        }
        Popup::ccTouchEnded(t, e);
    }

    void ccTouchCancelled(CCTouch* t, CCEvent* e) override {
        m_scrollTouchActive = false;
        m_scrollDragging = false;
        Popup::ccTouchCancelled(t, e);
    }
};

} // namespace decomind

#include "OfficialColorPickerPopup.hpp"

namespace {

static const char* SIDEBAR_NAMES[5] = {
    "INICIO", "INFORMACION", "CHAT", "ESTADISTICAS", "AJUSTES"
};

static const char* BOTTOM_NAMES[5] = {
    "FAVORITOS", "TUTORIALES", "MIS NIVELES", "GUARDAR", "REINICIAR"
};

static void fitBMFontLabel(CCLabelBMFont* lbl, std::string const& text, float maxW, float scale, bool isTitle = false) {
    if (!lbl) return;
    std::string clean = ThemeConfig::formatTextForActiveFont(text);
    lbl->setString(clean.c_str());
    float finalScale = scale * ThemeConfig::getActiveFontScaleMul(isTitle);
    lbl->setScale(finalScale);
    lbl->limitLabelWidth(maxW, finalScale, 0.10f);
}

static std::string colorToHexStr(ccColor3B c) {
    return fmt::format("#{:02X}{:02X}{:02X}", static_cast<int>(c.r), static_cast<int>(c.g), static_cast<int>(c.b));
}

// ===========================================================================
// Ventana Secundaria 1: Historial de Peticiones
// ===========================================================================
class HistoryPopup : public DMModalBase {
protected:
    std::function<void()> m_onClear;
    ScrollLayer* m_scroll = nullptr;

    bool init(std::function<void()> onClear) {
        if (!this->initModal(340.f, 240.f, "HISTORIAL DE PETICIONES")) return false;
        m_onClear = std::move(onClear);

        auto pal = ThemeConfig::getPalette();
        auto boxDn = CCDrawNode::create();
        ui::drawRoundedRect(
            boxDn, 12.f, 44.f, 316.f, 156.f, 6.f,
            rgba4F(pal.card, 0.96f), 1.0f, rgba4F(pal.line, 0.85f)
        );
        m_mainLayer->addChild(boxDn, 2);

        m_scroll = ScrollLayer::create(CCSize{304.f, 146.f});
        m_scroll->ignoreAnchorPointForPosition(false);
        m_scroll->setPosition({170.f, 122.f});
        m_mainLayer->addChild(m_scroll, 5);
        m_modalScroll = m_scroll;

        auto clearSpr = this->makeStyledActionBtn("BORRAR HISTORIAL", 136.f, 22.f, pal.elevated, COL_STATUS_RED);
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
        auto pal = ThemeConfig::getPalette();
        auto layer = m_scroll->m_contentLayer;
        layer->removeAllChildren();

        auto const& h = HistoryManager::entries();
        float viewW = 304.f;
        float viewH = 146.f;

        if (h.empty()) {
            layer->setContentSize({viewW, viewH});
            if (auto emptyLbl = ui::createStyledLabel("Aun no hay peticiones registradas.", false, 0.52f, pal.text2)) {
                emptyLbl->setPosition({viewW * 0.5f, viewH * 0.5f});
                layer->addChild(emptyLbl);
            }
            m_scroll->scrollToTop();
            return;
        }

        float itemH = 30.f;
        float gap = 4.f;
        float total = 8.f + static_cast<float>(h.size()) * (itemH + gap);
        total = std::max(viewH, total);
        layer->setContentSize({viewW, total});

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
            if (auto lPrompt = ui::createStyledLabel(sentence, false, 0.48f, pal.text, 212.f)) {
                lPrompt->setAnchorPoint({0.f, 0.5f});
                lPrompt->setPosition({8.f, itemH * 0.56f});
                node->addChild(lPrompt, 2);
            }

            if (auto lTime = ui::createStyledLabel(relTime, false, 0.42f, pal.text3, 68.f)) {
                lTime->setAnchorPoint({1.f, 0.5f});
                lTime->setPosition({288.f, itemH * 0.56f});
                node->addChild(lTime, 2);
            }

            auto sepDn = CCDrawNode::create();
            sepDn->drawSegment(ccp(8.f, 1.f), ccp(288.f, 1.f), 0.5f, rgba4F(pal.line, 0.65f));
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

        auto pal = ThemeConfig::getPalette();
        auto boxDn = CCDrawNode::create();
        ui::drawRoundedRect(
            boxDn, 12.f, 44.f, 316.f, 156.f, 6.f,
            rgba4F(pal.card, 0.96f), 1.0f, rgba4F(pal.line, 0.85f)
        );
        m_mainLayer->addChild(boxDn, 2);

        m_scroll = ScrollLayer::create(CCSize{304.f, 146.f});
        m_scroll->ignoreAnchorPointForPosition(false);
        m_scroll->setPosition({170.f, 122.f});
        m_mainLayer->addChild(m_scroll, 5);
        m_modalScroll = m_scroll;

        auto saveSpr = this->makeStyledActionBtn(
            "Guardar comando actual en Favoritos", 220.f, 22.f, pal.accent, pal.accent, true
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
        auto pal = ThemeConfig::getPalette();
        auto layer = m_scroll->m_contentLayer;
        layer->removeAllChildren();

        auto const& favs = HistoryManager::favorites();
        float viewW = 304.f;
        float viewH = 146.f;

        if (favs.empty()) {
            layer->setContentSize({viewW, viewH});
            if (auto lbl = ui::createStyledLabel("No tienes favoritos guardados.", false, 0.52f, pal.text2)) {
                lbl->setPosition({viewW * 0.5f, viewH * 0.5f});
                layer->addChild(lbl);
            }
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
            ccColor3B accent = (mi >= 0) ? pal.modeAccentSoft(mi) : pal.accentSoft;
            std::string modeName = (mi >= 0) ? MODES[mi].name : "General";

            auto node = CCNode::create();
            node->setContentSize({256.f, itemH});
            auto dn = CCDrawNode::create();
            ui::drawRoundedRect(
                dn, 0.f, 0.f, 256.f, itemH, 5.f,
                rgba4F(pal.elevated, 0.96f), 0.9f, rgba4F(pal.lineStrong, 0.85f)
            );
            node->addChild(dn, 1);

            if (auto l1 = ui::createStyledLabel(modeName, true, 0.26f, accent, 236.f)) {
                l1->setAnchorPoint({0.f, 0.5f});
                l1->setPosition({10.f, 24.f});
                node->addChild(l1, 2);
            }

            if (auto l2 = ui::createStyledLabel(f.prompt, false, 0.46f, pal.text, 236.f)) {
                l2->setAnchorPoint({0.f, 0.5f});
                l2->setPosition({10.f, 10.f});
                node->addChild(l2, 2);
            }

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

            auto delNode = this->makeStyledActionBtn("X", 26.f, itemH - 4.f, pal.card, COL_STATUS_RED);
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

        auto pal = ThemeConfig::getPalette();
        auto boxDn = CCDrawNode::create();
        ui::drawRoundedRect(
            boxDn, 12.f, 16.f, 326.f, 184.f, 6.f,
            rgba4F(pal.card, 0.96f), 1.0f, rgba4F(pal.line, 0.85f)
        );
        m_mainLayer->addChild(boxDn, 2);

        auto scroll = ScrollLayer::create(CCSize{310.f, 172.f});
        scroll->ignoreAnchorPointForPosition(false);
        scroll->setPosition({175.f, 108.f});
        m_mainLayer->addChild(scroll, 5);
        m_modalScroll = scroll;

        std::string guideText =
            "1. COMO USAR DECOMIND AI EN EL EDITOR\n"
            "- En INICIO, desliza con el dedo o raton y toca uno de los 9 modos.\n"
            "- En INFORMACION puedes consultar que objetos usa cada modo y probar ejemplos.\n"
            "- En CHAT escribe tu instruccion (acepta ñ y acentos) o toca uno de los chips rapidos.\n\n"
            "2. SISTEMA CROMATICO (UN COLOR -> TODA LA PALETA)\n"
            "- En AJUSTES elige cualquiera de los 8 tonos o mueve la Rueda Oficial de GD.\n"
            "- Todo el menu se recolorea automaticamente en esa escala cromatica manteniendo contraste perfecto.\n\n"
            "3. FUENTES PERSONALIZADAS DEL JUEGO\n"
            "- En AJUSTES, dentro de 'Fuente de letras', despliega la flecha hacia arriba para elegir cualquiera de las 62 fuentes del juego.";

        auto holder = CCNode::create();
        auto area = SimpleTextArea::create(guideText, "chatFont.fnt", 0.50f, 294.f);
        area->setWrappingMode(WORD_WRAP);
        area->setColor(rgba4B(pal.text, 255));
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

        auto pal = ThemeConfig::getPalette();
        std::string title = LevelExecutor::getLevelTitle(lel);
        int count = LevelExecutor::getCurrentObjectCount(lel);

        m_infoLbl = ui::createStyledLabel(
            fmt::format("Nivel en editor: {}  |  Objetos: {}", title, count),
            false, 0.50f, pal.accentSoft, 312.f
        );
        if (m_infoLbl) {
            m_infoLbl->setPosition({170.f, 194.f});
            m_mainLayer->addChild(m_infoLbl, 10);
        }

        auto boxDn = CCDrawNode::create();
        ui::drawRoundedRect(
            boxDn, 12.f, 44.f, 316.f, 138.f, 6.f,
            rgba4F(pal.card, 0.96f), 1.0f, rgba4F(pal.line, 0.85f)
        );
        m_mainLayer->addChild(boxDn, 2);

        m_scroll = ScrollLayer::create(CCSize{304.f, 128.f});
        m_scroll->ignoreAnchorPointForPosition(false);
        m_scroll->setPosition({170.f, 113.f});
        m_mainLayer->addChild(m_scroll, 5);
        m_modalScroll = m_scroll;

        auto snapSpr = this->makeStyledActionBtn("Registrar estado actual", 196.f, 22.f, pal.accent, pal.accent, true);
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
        auto pal = ThemeConfig::getPalette();
        auto layer = m_scroll->m_contentLayer;
        layer->removeAllChildren();

        auto snaps = LevelExecutor::listSnapshots();
        float viewW = 304.f;
        float viewH = 128.f;

        if (snaps.empty()) {
            layer->setContentSize({viewW, viewH});
            if (auto lbl = ui::createStyledLabel("Aun no hay puntos de guardado registrados.", false, 0.50f, pal.text2)) {
                lbl->setPosition({viewW * 0.5f, viewH * 0.5f});
                layer->addChild(lbl);
            }
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
                rgba4F(pal.elevated, 0.96f), 0.9f, rgba4F(pal.lineStrong, 0.8f)
            );
            node->addChild(dn, 1);

            if (auto l1 = ui::createStyledLabel(fmt::format("{} ({} obj)", s.levelName, s.objectCount), true, 0.26f, pal.text, 274.f)) {
                l1->setAnchorPoint({0.f, 0.5f});
                l1->setPosition({10.f, 22.f});
                node->addChild(l1, 2);
            }

            if (auto l2 = ui::createStyledLabel(s.dateStr, false, 0.44f, pal.text2, 274.f)) {
                l2->setAnchorPoint({0.f, 0.5f});
                l2->setPosition({10.f, 9.f});
                node->addChild(l2, 2);
            }

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
// Delegado de entrada de texto para el cuadro de CHAT / COMANDO:
//   - Corrige el bug de CCTextInputNode::onTextFieldInsertText (0x5a4498)
//   - Soporta ñ, Ñ y vocales acentuadas sin borrar el texto anterior.
// ===========================================================================
class PromptTextFieldDelegate : public cocos2d::CCTextFieldDelegate {
public:
    TextInput* m_textInput = nullptr;
    CCTextInputNode* m_inputNode = nullptr;
    cocos2d::CCTextFieldTTF* m_textField = nullptr;
    cocos2d::CCTextFieldDelegate* m_origDelegate = nullptr;
    std::function<void(std::string const&)> m_onTextUpdated;
    bool m_internalSync = false;

    void bind(
        TextInput* ti,
        CCTextInputNode* inNode,
        cocos2d::CCTextFieldTTF* tf,
        cocos2d::CCTextFieldDelegate* origDel,
        std::function<void(std::string const&)> onUpdated
    ) {
        m_textInput = ti;
        m_inputNode = inNode;
        m_textField = tf;
        m_origDelegate = origDel;
        m_onTextUpdated = std::move(onUpdated);
        if (m_textField) {
            m_textField->setDelegate(this);
        }
    }

    void unbind() {
        if (m_textField && m_origDelegate) {
            m_textField->setDelegate(m_origDelegate);
        }
        m_textField = nullptr;
        m_origDelegate = nullptr;
        m_inputNode = nullptr;
        m_textInput = nullptr;
    }

    bool onTextFieldAttachWithIME(cocos2d::CCTextFieldTTF* sender) override {
        if (m_origDelegate && m_origDelegate != this) {
            return m_origDelegate->onTextFieldAttachWithIME(sender);
        }
        return false;
    }

    bool onTextFieldDetachWithIME(cocos2d::CCTextFieldTTF* sender) override {
        if (m_origDelegate && m_origDelegate != this) {
            return m_origDelegate->onTextFieldDetachWithIME(sender);
        }
        return false;
    }

    bool onTextFieldInsertText(
        cocos2d::CCTextFieldTTF* sender,
        char const* text,
        int nLen,
        cocos2d::enumKeyCodes keyCodes
    ) override {
        if (!m_textInput || !m_inputNode || !text || nLen <= 0) {
            return true;
        }
        if (text[0] == '\n' || text[0] == '\r') {
            return true;
        }
        if (keyCodes == cocos2d::KEY_Home || keyCodes == cocos2d::KEY_End ||
            keyCodes == cocos2d::KEY_Left || keyCodes == cocos2d::KEY_Right) {
            return true;
        }

        std::string rawChunk(text, static_cast<size_t>(nLen));
        std::string norm = normalizePromptInputWithEnye(rawChunk);
        if (norm.empty()) {
            return true;
        }

        std::string cur = m_textInput->getString();
        constexpr size_t kMaxChars = 160;
        if (cur.size() >= kMaxChars) {
            return true;
        }
        if (cur.size() + norm.size() > kMaxChars) {
            norm.resize(kMaxChars - cur.size());
        }
        cur += norm;

        m_internalSync = true;
        m_textInput->setString(cur);
        m_internalSync = false;

        if (m_onTextUpdated) {
            m_onTextUpdated(cur);
        }
        return true;
    }

    bool onTextFieldDeleteBackward(
        cocos2d::CCTextFieldTTF* sender,
        char const* delText,
        int nLen
    ) override {
        if (!m_textInput || !m_inputNode) {
            return false;
        }
        std::string cur = m_textInput->getString();
        if (!cur.empty()) {
            cur.pop_back();
            m_internalSync = true;
            m_textInput->setString(cur);
            m_internalSync = false;
            if (m_onTextUpdated) {
                m_onTextUpdated(cur);
            }
        }
        return true;
    }

    void textChanged() override {
        if (m_origDelegate && m_origDelegate != this) {
            m_origDelegate->textChanged();
        }
        if (!m_internalSync && m_textInput && m_onTextUpdated) {
            m_onTextUpdated(m_textInput->getString());
        }
    }
};

// ===========================================================================
// Menu Principal de DecoMind AI v0.3.8-alpha (Fidelidad Total al PDF)
// ===========================================================================
class DecoMindPopup : public Popup, public cocos2d::extension::ColorPickerDelegate {
protected:
    Ref<LevelEditorLayer> m_lel;
    AIClient m_aiClient;

    int m_selectedMode = 0; // 0=Estructurar por defecto (como en Pagina 1 del PDF)
    int m_activePage = 0;   // 0=INICIO, 1=INFORMACION, 2=CHAT, 3=ESTADISTICAS, 4=AJUSTES
    int m_sessionRequests = 0;
    int m_sessionPlacedObjects = 0;
    bool m_canUndoAiAction = false;

    std::string m_lastUserBubbleText;
    std::string m_lastReplyText;
    std::string m_lastSummaryPill1;
    std::string m_lastSummaryPill2;
    std::string m_lastSummaryPill3;
    bool m_serverReachable = false;
    bool m_serverChecked = false;
    float m_connectionCheckTimer = 15.f;
    std::string m_savedPromptText;

    float m_animTime = 0.f;
    float m_wormPhase = 0.f;

    // Nodos de estructura general
    CCMenu* m_fixedMenu = nullptr;
    CCMenu* m_sidebarMenu = nullptr;
    CCDrawNode* m_outerFrameNode = nullptr;
    CCDrawNode* m_spikesRgbNode = nullptr;
    CCNode* m_mascotTop = nullptr;
    float m_headerRobotBaseScale = 1.f;

    // Header dinamico (cambia titulo y subtitulo segun la pagina activa, igual al PDF)
    CCLabelBMFont* m_headerTitleLbl = nullptr;
    CCLabelBMFont* m_headerSubtitleLbl = nullptr;
    CCDrawNode* m_connPillDn = nullptr;
    CCLabelBMFont* m_connPillLbl = nullptr;

    // Contenedores de las 5 paginas
    CCNode* m_centerPages[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};

    // --- PAGINA 0: INICIO (Cuadricula 3x3 + Columna Derecha) ---
    ScrollLayer* m_inicioScroll = nullptr;
    CCNode* m_cardNodes[9] = {nullptr};
    CCDrawNode* m_cardBorderNodes[9] = {nullptr};
    CCDrawNode* m_inicioScrollBarNode = nullptr;
    float m_inicioMinScrollY = 0.f;
    float m_savedInicioScrollY = 99999.f;

    // Columna Derecha de Pagina 0 (USO DE LA IA, CHAT / COMANDO, HISTORIAL)
    CCDrawNode* m_quotaBarFill = nullptr;
    CCLabelBMFont* m_quotaBigNumLbl = nullptr;
    CCLabelBMFont* m_quotaSubLbl = nullptr;
    CCLabelBMFont* m_reqDoneValLbl = nullptr;
    CCLabelBMFont* m_curLevelValLbl = nullptr;
    CCLabelBMFont* m_activeModeValLbl = nullptr;
    CCLabelBMFont* m_histRowText[3] = {nullptr, nullptr, nullptr};
    CCLabelBMFont* m_histRowTime[3] = {nullptr, nullptr, nullptr};

    // --- PAGINA 1: INFORMACION / MODOS (Lista izquierda + Detalle derecho) ---
    ScrollLayer* m_infoListScroll = nullptr;
    CCNode* m_infoRowNodes[9] = {nullptr};
    float m_infoMinScrollY = 0.f;
    float m_savedInfoScrollY = 99999.f;
    CCNode* m_infoDetailContainer = nullptr;

    // --- PAGINA 2: CHAT COMPLETO (Conversacion central + Percepcion derecha) ---
    CCLabelBMFont* m_chatModeBtnLbl = nullptr;
    CCDrawNode* m_chatStateBadgeDn = nullptr;
    CCLabelBMFont* m_chatStateBadgeLbl = nullptr;
    CCLabelBMFont* m_chatUserBubbleLbl = nullptr;
    CCLabelBMFont* m_chatAiBubbleLbl = nullptr;
    ScrollLayer* m_chatScroll = nullptr;
    CCLabelBMFont* m_chatPillLbls[3] = {nullptr, nullptr, nullptr};
    CCLabelBMFont* m_chatRightLevelLbl = nullptr;
    CCLabelBMFont* m_chatRightSelLbl = nullptr;
    CCLabelBMFont* m_chatRightZoneLbl = nullptr;
    CCLabelBMFont* m_chatRightCatLbl = nullptr;
    CCLabelBMFont* m_chatRightNextGroupLbl = nullptr;
    CCLabelBMFont* m_chatRightNextColorLbl = nullptr;
    CCLabelBMFont* m_chatHistRowText[3] = {nullptr, nullptr, nullptr};
    CCLabelBMFont* m_chatHistRowTime[3] = {nullptr, nullptr, nullptr};

    // Widget de Entrada de Texto compartido entre Pagina 0 y Pagina 2
    CCNode* m_chatInputWidget = nullptr;
    CCDrawNode* m_chatInputBgDn = nullptr;
    TextInput* m_promptInput = nullptr;
    PromptTextFieldDelegate m_promptFieldDelegate;
    CCNode* m_placeholderNode = nullptr;
    CCLabelBMFont* m_typedPreviewLines[4] = {nullptr, nullptr, nullptr, nullptr};
    CCNode* m_tildeOverlayNode = nullptr;
    CCLabelBMFont* m_charCountLbl = nullptr;
    CCMenuItemSpriteExtra* m_sendBtnItem = nullptr;

    // --- PAGINA 3: ESTADISTICAS ---
    CCNode* m_statsContentHolder = nullptr;

    // --- PAGINA 4: AJUSTES + SELECTOR DE COLOR + DROPDOWN DE FUENTES ---
    ScrollLayer* m_settingsScroll = nullptr;
    float m_settingsMinScrollY = 0.f;
    float m_savedSettingsScrollY = 99999.f;
    TextInput* m_serverUrlInput = nullptr;
    CCLabelBMFont* m_settingsStatusLbl = nullptr;

    // Selector de color integrado a la derecha de Ajustes (Pagina 5)
    int m_colorEditTarget = 0; // 0 = Color del menu, 1 = Bordes/Pinchos RGB, 2 = Robot RGB
    cocos2d::extension::CCControlColourPicker* m_embeddedWheel = nullptr;
    CCDrawNode* m_embeddedPreviewDn = nullptr;
    TextInput* m_embRInput = nullptr;
    TextInput* m_embGInput = nullptr;
    TextInput* m_embBInput = nullptr;
    TextInput* m_embHexInput = nullptr;
    CCLabelBMFont* m_embTargetTitleLbl = nullptr;
    ccColor3B m_embCurColor{};
    bool m_embUpdatingInputs = false;

    // Menu desplegable hacia arriba para "Fuente de letras"
    CCNode* m_fontDropdownOverlay = nullptr;
    ScrollLayer* m_fontDropdownScroll = nullptr;
    bool m_fontDropdownOpen = false;
    float m_fontDropMinScrollY = 0.f;

    // Overlay de "Cargando fuente..." con desenfoque/oscurecimiento suave y circulo giratorio
    CCNode* m_fontLoadingOverlay = nullptr;
    float m_fontLoadingRemaining = 0.f;
    int m_pendingFontIndex = -1;
    bool m_pendingRebuild = false;

    void scheduleRebuild() {
        m_pendingRebuild = true;
    }

    // Estados de interaccion tactil (deslizar con el dedo vs tocar)
    int m_touchScrollTarget = 0; // 0=ninguno, 1=Inicio cards, 2=Info list, 3=Ajustes list, 4=Font dropdown
    bool m_touchDragging = false;
    int m_pressedItemIdx = -1;
    CCPoint m_touchStartPt{0.f, 0.f};
    float m_touchStartContentY = 0.f;

    // Banner inferior derecho del robot aliado
    CCDrawNode* m_allyBgNode = nullptr;
    CCDrawNode* m_allyBorderNode = nullptr;
    CCNode* m_allyRobotNode = nullptr;
    float m_allyRobotBaseScale = 1.f;
    CCLabelBMFont* m_allyLine1 = nullptr;
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

        m_embCurColor = ThemeConfig::getMenuAccent();

        this->rebuildAllUIContent();
        this->schedule(schedule_selector(DecoMindPopup::tick));
        return true;
    }

    // Reconstruye toda la interfaz aplicando al instante la paleta cromatica (Pagina 6)
    // y la fuente de letras activa en cada etiqueta, tarjeta y panel.
    void rebuildAllUIContent() {
        if (m_promptInput) {
            m_savedPromptText = m_promptInput->getString();
        }
        if (m_inicioScroll && m_inicioScroll->m_contentLayer) {
            m_savedInicioScrollY = m_inicioScroll->m_contentLayer->getPositionY();
        }
        if (m_infoListScroll && m_infoListScroll->m_contentLayer) {
            m_savedInfoScrollY = m_infoListScroll->m_contentLayer->getPositionY();
        }
        if (m_settingsScroll && m_settingsScroll->m_contentLayer) {
            m_savedSettingsScrollY = m_settingsScroll->m_contentLayer->getPositionY();
        }

        m_promptFieldDelegate.unbind();
        m_mainLayer->removeAllChildren();

        // Limpiar punteros
        m_fixedMenu = nullptr;
        m_sidebarMenu = nullptr;
        m_outerFrameNode = nullptr;
        m_spikesRgbNode = nullptr;
        m_mascotTop = nullptr;
        m_headerTitleLbl = nullptr;
        m_headerSubtitleLbl = nullptr;
        m_connPillDn = nullptr;
        m_connPillLbl = nullptr;
        for (int i = 0; i < 5; ++i) m_centerPages[i] = nullptr;
        for (int i = 0; i < 9; ++i) {
            m_cardNodes[i] = nullptr;
            m_cardBorderNodes[i] = nullptr;
            m_infoRowNodes[i] = nullptr;
        }
        m_inicioScroll = nullptr;
        m_inicioScrollBarNode = nullptr;
        m_infoListScroll = nullptr;
        m_infoDetailContainer = nullptr;
        m_settingsScroll = nullptr;
        m_serverUrlInput = nullptr;
        m_settingsStatusLbl = nullptr;
        m_embeddedWheel = nullptr;
        m_embeddedPreviewDn = nullptr;
        m_embRInput = nullptr;
        m_embGInput = nullptr;
        m_embBInput = nullptr;
        m_embHexInput = nullptr;
        m_embTargetTitleLbl = nullptr;
        m_fontDropdownOverlay = nullptr;
        m_fontDropdownScroll = nullptr;
        m_fontDropdownOpen = false;
        m_chatScroll = nullptr;
        m_fontLoadingOverlay = nullptr;

        m_outerFrameNode = CCDrawNode::create();
        m_mainLayer->addChild(m_outerFrameNode, 1);

        m_spikesRgbNode = CCDrawNode::create();
        m_mainLayer->addChild(m_spikesRgbNode, 6);

        m_fixedMenu = CCMenu::create();
        m_fixedMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_fixedMenu, 60);

        m_sidebarMenu = CCMenu::create();
        m_sidebarMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(m_sidebarMenu, 55);

        this->drawMainOuterShell();
        this->buildTopHeader();
        this->buildSidebar();

        for (int i = 0; i < 5; ++i) {
            m_centerPages[i] = CCNode::create();
            m_mainLayer->addChild(m_centerPages[i], 10);
        }

        this->buildPage0Inicio();
        this->buildPage1Informacion();
        this->buildPage2Chat();
        this->buildPage3Estadisticas();
        this->buildPage4Ajustes();
        this->buildSharedChatInputWidget();
        this->buildBottomBar();

        if (!m_savedPromptText.empty()) {
            this->setPromptText(m_savedPromptText);
        }
        this->selectMode(m_selectedMode);
        this->showPage(m_activePage);
        this->refreshUsagePanel();
        this->refreshHistoryMiniPanel();
        this->updateAllyBannerText();

        if (auto uiLayer = EditorUI::get()) {
            if (auto btn = typeinfo_cast<DragButton*>(uiLayer->getChildByID("decomind-menu"_spr))) {
                btn->refreshTheme();
            }
        }
    }

    // Dibuja el marco exterior, barra lateral izquierda y linea superior (fiel a las Paginas 1-5)
    void drawMainOuterShell() {
        if (!m_outerFrameNode) return;
        m_outerFrameNode->clear();
        auto pal = ThemeConfig::getPalette();

        // 1. Fondo exterior (50/5) con borde fino Linea (30/22)
        ui::drawRoundedRect(
            m_outerFrameNode, 0.f, 0.f, PW, PH, ps(22.f),
            rgba4F(pal.bgOuter, 0.99f),
            ps(2.0f),
            rgba4F(pal.line, 0.95f)
        );

        // 2. Linea divisoria superior horizontal (cortada por el robot central)
        m_outerFrameNode->drawSegment(
            ccp(px(28.f), py(114.f)),
            ccp(px(1508.f), py(114.f)),
            ps(1.2f),
            rgba4F(pal.line, 0.85f)
        );

        // 3. Panel de la barra lateral izquierda (x_ref=28..236, y_top_ref=128..880)
        ui::drawRoundedRect(
            m_outerFrameNode,
            px(28.f), py(880.f), ps(208.f), ps(752.f), ps(18.f),
            rgba4F(pal.panel, 0.98f),
            ps(1.4f),
            rgba4F(pal.line, 0.90f)
        );
    }

    // -----------------------------------------------------------------------
    // 1. HEADER SUPERIOR: "<", Titulo + Subtitulo, Robot central y Pildoras derechas
    // -----------------------------------------------------------------------
    void buildTopHeader() {
        auto pal = ThemeConfig::getPalette();

        auto backNode = ui::createBackArrowIcon();
        auto backBtn = CCMenuItemExt::createSpriteExtra(
            backNode, [this](CCMenuItemSpriteExtra*) { this->onClose(nullptr); }
        );
        backBtn->m_scaleMultiplier = 1.04f;
        backBtn->setPosition({px(54.f), py(66.f)});
        m_fixedMenu->addChild(backBtn);

        m_headerTitleLbl = ui::createStyledLabel("DecoMind AI", true, 0.42f, pal.text, ps(420.f));
        if (m_headerTitleLbl) {
            m_headerTitleLbl->setAnchorPoint({0.f, 0.5f});
            m_headerTitleLbl->setPosition({px(92.f), py(52.f)});
            m_mainLayer->addChild(m_headerTitleLbl, 15);
        }

        m_headerSubtitleLbl = ui::createStyledLabel("", false, 0.44f, pal.text3, ps(480.f));
        if (m_headerSubtitleLbl) {
            m_headerSubtitleLbl->setAnchorPoint({0.f, 0.5f});
            m_headerSubtitleLbl->setPosition({px(92.f), py(84.f)});
            m_mainLayer->addChild(m_headerSubtitleLbl, 15);
        }

        // Mascota robot centrada en x_ref = 768 sobre la linea divisoria
        m_mascotTop = ui::createRobotMascotNode(true);
        if (auto face = typeinfo_cast<CCSprite*>(m_mascotTop->getChildByTag(301)))
            m_headerRobotBaseScale = face->getScale();
        auto mascotBtn = CCMenuItemExt::createSpriteExtra(
            m_mascotTop, [this](CCMenuItemSpriteExtra*) { this->nextAllyPhrase(); }
        );
        mascotBtn->m_scaleMultiplier = 1.03f;
        mascotBtn->setPosition({px(768.f), py(64.f)});
        m_fixedMenu->addChild(mascotBtn);

        // Pildora de estado de conexion (derecha) + Pildora de version ("v0.3.8 a")
        m_connPillDn = CCDrawNode::create();
        m_mainLayer->addChild(m_connPillDn, 14);

        m_connPillLbl = ui::createStyledLabel("", false, 0.42f, pal.text2, ps(186.f));
        if (m_connPillLbl) {
            m_connPillLbl->setAnchorPoint({0.f, 0.5f});
            m_connPillLbl->setPosition({px(1224.f), py(66.f)});
            m_mainLayer->addChild(m_connPillLbl, 15);
        }

        // Estado informativo; el cambio de modo se hace exclusivamente en Ajustes.

        ui::addRefRoundedBox(
            m_mainLayer,
            1426.f, 47.f, 82.f, 38.f, 12.f,
            pal.card, 0.98f, pal.line, 0.90f, 1.2f, 14
        );
        if (auto verLbl = ui::createStyledLabel("v0.3.8 a", false, 0.40f, pal.text2, ps(74.f))) {
            verLbl->setPosition({px(1467.f), py(66.f)});
            m_mainLayer->addChild(verLbl, 15);
        }

        this->refreshHeaderStatusPill();
    }

    void refreshHeaderStatusPill() {
        if (!m_connPillDn) return;
        m_connPillDn->clear();
        auto pal = ThemeConfig::getPalette();
        bool isLocal = (AIClient::getOriginMode() == "local");

        ui::drawRoundedRect(
            m_connPillDn,
            px(1194.f), py(85.f), ps(222.f), ps(38.f), ps(12.f),
            rgba4F(pal.card, 0.98f),
            ps(1.2f),
            rgba4F(pal.line, 0.90f)
        );
        ccColor3B dotCol = isLocal ? COL_STATUS_AMBER : (m_serverReachable ? COL_STATUS_GREEN : COL_STATUS_RED);
        m_connPillDn->drawDot(ccp(px(1211.f), py(66.f)), ps(5.0f), rgba4F(dotCol, 1.f));

        if (m_connPillLbl) {
            fitBMFontLabel(
                m_connPillLbl,
                isLocal ? "Modo local activo" : (!m_serverChecked ? "Comprobando servidor" : (m_serverReachable ? "Servidor conectado" : "Servidor sin conexion")),
                ps(184.f), 0.41f
            );
            m_connPillLbl->setColor(pal.text2);
        }
    }

    void updateHeaderTitleForPage(int p) {
        if (!m_headerTitleLbl || !m_headerSubtitleLbl) return;
        std::string lvlName = LevelExecutor::getLevelTitle(m_lel);
        switch (p) {
            case 0:
                fitBMFontLabel(m_headerTitleLbl, "DecoMind AI", ps(420.f), 0.42f, true);
                fitBMFontLabel(m_headerSubtitleLbl, "Editor de niveles - " + lvlName, ps(480.f), 0.44f, false);
                break;
            case 1:
                fitBMFontLabel(m_headerTitleLbl, "Informacion", ps(420.f), 0.42f, true);
                fitBMFontLabel(m_headerSubtitleLbl, "Que hace cada modo y como pedirlo", ps(480.f), 0.44f, false);
                break;
            case 2:
                fitBMFontLabel(m_headerTitleLbl, "Chat", ps(420.f), 0.42f, true);
                fitBMFontLabel(m_headerSubtitleLbl, "Pidele a la IA lo que quieres en tu nivel", ps(480.f), 0.44f, false);
                break;
            case 3:
                fitBMFontLabel(m_headerTitleLbl, "Estadisticas", ps(420.f), 0.42f, true);
                fitBMFontLabel(m_headerSubtitleLbl, "Tu uso de la IA y el estado del nivel", ps(480.f), 0.44f, false);
                break;
            default:
                fitBMFontLabel(m_headerTitleLbl, "Ajustes", ps(420.f), 0.42f, true);
                fitBMFontLabel(m_headerSubtitleLbl, "Conexion, color del menu y boton del editor", ps(480.f), 0.44f, false);
                break;
        }
    }

    // -----------------------------------------------------------------------
    // 2. BARRA LATERAL IZQUIERDA (5 pestanas)
    // -----------------------------------------------------------------------
    void buildSidebar() {
        if (!m_sidebarMenu) return;
        m_sidebarMenu->removeAllChildren();
        const float yCenters[5] = {174.f, 250.f, 326.f, 402.f, 478.f};

        for (int i = 0; i < 5; ++i) {
            bool isAct = (i == m_activePage);
            char const* tabTitle = SIDEBAR_NAMES[i];
            if (i == 1 && m_activePage >= 2) {
                tabTitle = "MODOS"; // En Paginas 3-5 del PDF el segundo tab muestra MODOS
            }
            auto btnNode = ui::createSidebarButtonNode(i, tabTitle, isAct);
            auto btn = CCMenuItemExt::createSpriteExtra(
                btnNode, [this, i](CCMenuItemSpriteExtra*) { this->showPage(i); }
            );
            btn->m_scaleMultiplier = 1.02f;
            btn->setPosition({px(132.f), py(yCenters[i])});
            m_sidebarMenu->addChild(btn);
        }
    }

    void showPage(int p) {
        if (p < 0 || p >= 5) p = 0;
        m_activePage = p;
        if (m_fontDropdownOverlay) {
            m_fontDropdownOverlay->setVisible(false);
            m_fontDropdownOpen = false;
        }
        for (int i = 0; i < 5; ++i) {
            if (m_centerPages[i]) {
                m_centerPages[i]->setVisible(i == p);
            }
        }
        this->buildSidebar();
        this->updateHeaderTitleForPage(p);
        this->refreshHeaderStatusPill();

        if (m_serverUrlInput) {
            m_serverUrlInput->setEnabled(p == 4);
        }
        if (m_embRInput) m_embRInput->setEnabled(p == 4);
        if (m_embGInput) m_embGInput->setEnabled(p == 4);
        if (m_embBInput) m_embBInput->setEnabled(p == 4);
        if (m_embHexInput) m_embHexInput->setEnabled(p == 4);

        // El widget de entrada de texto del chat es visible en Pagina 0 (columna derecha)
        // y en Pagina 2 (panel central inferior de conversacion)
        if (m_chatInputWidget) {
            if (p == 0) {
                m_chatInputWidget->setVisible(true);
                m_chatInputWidget->setPosition({px(1308.f), py(546.f)});
                m_chatInputWidget->setScale(1.0f);
            } else if (p == 2) {
                m_chatInputWidget->setVisible(true);
                m_chatInputWidget->setPosition({px(695.f), py(782.f)});
                m_chatInputWidget->setScale(1.0f);
            } else {
                m_chatInputWidget->setVisible(false);
            }
            this->relayoutChatInputWidget(p == 2);
        }

        if (p == 1) this->refreshPage1Detail();
        if (p == 2) this->refreshPage2ChatPerception();
        if (p == 3) this->refreshPage3Stats();
    }

    // -----------------------------------------------------------------------
    // 3. PAGINA 0 (INICIO - Pagina 1 del PDF):
    //    Cuadricula 3x3 deslizable + Columna Derecha (USO DE LA IA, CHAT, HISTORIAL)
    // -----------------------------------------------------------------------
    void buildPage0Inicio() {
        auto page = m_centerPages[0];
        auto pal = ThemeConfig::getPalette();

        if (auto hdrLeft = ui::createStyledLabel("Elige un modo", true, 0.34f, pal.text, ps(320.f))) {
            hdrLeft->setAnchorPoint({0.f, 0.5f});
            hdrLeft->setPosition({px(262.f), py(148.f)});
            page->addChild(hdrLeft, 5);
        }
        if (auto hdrRight = ui::createStyledLabel("Toca un modo y escribe en el chat que quieres lograr", false, 0.42f, pal.text3, ps(480.f))) {
            hdrRight->setAnchorPoint({1.f, 0.5f});
            hdrRight->setPosition({px(1086.f), py(148.f)});
            page->addChild(hdrRight, 5);
        }

        float viewW = ps(836.f);
        float viewH = ps(700.f);
        m_inicioScroll = ScrollLayer::create(CCSize{viewW, viewH});
        m_inicioScroll->ignoreAnchorPointForPosition(false);
        m_inicioScroll->setAnchorPoint({0.5f, 0.5f});
        m_inicioScroll->setPosition({px(674.f), py(526.f)});
        page->addChild(m_inicioScroll, 5);

        m_inicioScrollBarNode = CCDrawNode::create();
        page->addChild(m_inicioScrollBarNode, 12);

        auto content = m_inicioScroll->m_contentLayer;
        float totalH = viewH + ps(92.f);
        content->setContentSize({viewW, totalH});
        m_inicioMinScrollY = viewH - totalH;

        const float colLocalX[3] = {ps(4.f), ps(284.f), ps(564.f)};
        const float cardW = ps(264.f);
        const float cardH = ps(214.f);
        const float rowGap = ps(18.f);
        const float topMargin = ps(8.f);

        for (int i = 0; i < 9; ++i) {
            int r = i / 3;
            int c = i % 3;
            float lx = colLocalX[c];
            float ly = totalH - topMargin - cardH - r * (cardH + rowGap);
            auto const& m = MODES[i];
            bool isSel = (i == m_selectedMode);

            auto cardNode = CCNode::create();
            cardNode->setContentSize({cardW, cardH});
            cardNode->ignoreAnchorPointForPosition(false);
            cardNode->setAnchorPoint({0.5f, 0.5f});
            cardNode->setPosition({lx + cardW * 0.5f, ly + cardH * 0.5f});

            // 1. Fondo de tarjeta (Tarjeta 36/12 o Elevado 32/17 cuando esta activa)
            auto bgDn = CCDrawNode::create();
            bgDn->setTag(790);
            ui::drawRoundedRect(
                bgDn, 0.f, 0.f, cardW, cardH, ps(16.f),
                rgba4F(isSel ? pal.elevated : pal.card, 0.98f),
                ps(isSel ? 1.8f : 1.1f),
                rgba4F(isSel ? pal.accent : pal.line, isSel ? 0.95f : 0.85f)
            );
            cardNode->addChild(bgDn, 1);

            // 2. Contorno de gusano RGB
            auto borderDn = CCDrawNode::create();
            cardNode->addChild(borderDn, 2);
            m_cardBorderNodes[i] = borderDn;

            // 3. Recuadro de icono arriba a la izquierda (48x48 ref)
            auto iconBadge = ui::createModeIconBadge(i, isSel, 50.f, 13.f);
            iconBadge->setTag(791);
            iconBadge->ignoreAnchorPointForPosition(false);
            iconBadge->setAnchorPoint({0.f, 1.f});
            iconBadge->setPosition({ps(22.f), cardH - ps(22.f)});
            cardNode->addChild(iconBadge, 4);

            // 4. Circulo de check arriba a la derecha cuando la tarjeta esta seleccionada (fiel a Pagina 1)
            if (isSel) {
                auto chkDn = CCDrawNode::create();
                chkDn->setTag(792);
                CCPoint chkCenter = ccp(cardW - ps(34.f), cardH - ps(46.f));
                ui::drawCircleShape(
                    chkDn, chkCenter, ps(12.f),
                    rgba4F(pal.accent, 1.f), 0.f, ccc4f(0, 0, 0, 0), 20
                );
                cardNode->addChild(chkDn, 4);
                if (auto chkSpr = ui::createFitSprite("ui-check.png"_spr, ps(13.f), ps(13.f), chkCenter)) {
                    chkSpr->setColor(pal.inkOnAccent);
                    chkSpr->setTag(793);
                    cardNode->addChild(chkSpr, 5);
                }
            }

            // 5. Titulo en mayusculas alineado a la izquierda
            if (auto titleLbl = ui::createStyledLabel(m.title, true, 0.25f, pal.text, cardW - ps(40.f))) {
                titleLbl->setAnchorPoint({0.f, 0.5f});
                titleLbl->setPosition({ps(22.f), cardH * 0.45f});
                cardNode->addChild(titleLbl, 5);
            }

            // 6. Dos lineas de descripcion en Texto 2 alineadas a la izquierda
            if (auto desc1 = ui::createStyledLabel(m.visibleLine1, false, 0.41f, pal.text2, cardW - ps(40.f))) {
                desc1->setAnchorPoint({0.f, 0.5f});
                desc1->setPosition({ps(22.f), cardH * 0.27f});
                cardNode->addChild(desc1, 5);
            }
            if (auto desc2 = ui::createStyledLabel(m.visibleLine2, false, 0.41f, pal.text2, cardW - ps(40.f))) {
                desc2->setAnchorPoint({0.f, 0.5f});
                desc2->setPosition({ps(22.f), cardH * 0.15f});
                cardNode->addChild(desc2, 5);
            }

            content->addChild(cardNode, 10);
            m_cardNodes[i] = cardNode;
        }

        if (m_savedInicioScrollY <= 0.f && m_savedInicioScrollY >= m_inicioMinScrollY) {
            content->setPositionY(m_savedInicioScrollY);
        } else {
            m_inicioScroll->scrollToTop();
        }
        this->updateInicioScrollbar();

        // --- COLUMNA DERECHA EN PAGINA 0 (x_ref = 1108..1508) ---
        // Panel 1: USO DE LA IA (y_top_ref = 128..398)
        ui::addRefRoundedBox(
            page, 1108.f, 128.f, 400.f, 270.f, 18.f,
            pal.panel, 0.98f, pal.line, 0.90f, 1.4f, 3
        );
        if (auto uTitle = ui::createStyledLabel("USO DE LA IA", true, 0.23f, pal.text2, ps(180.f))) {
            uTitle->setAnchorPoint({0.f, 0.5f});
            uTitle->setPosition({px(1132.f), py(158.f)});
            page->addChild(uTitle, 5);
        }
        if (auto uReset = ui::createStyledLabel("Se reinicia a medianoche", false, 0.38f, pal.text3, ps(180.f))) {
            uReset->setAnchorPoint({1.f, 0.5f});
            uReset->setPosition({px(1484.f), py(180.f)});
            page->addChild(uReset, 5);
        }

        m_quotaBigNumLbl = ui::createStyledLabel("0", true, 0.54f, pal.accentSoft, ps(90.f));
        if (m_quotaBigNumLbl) {
            m_quotaBigNumLbl->setAnchorPoint({0.f, 0.5f});
            m_quotaBigNumLbl->setPosition({px(1132.f), py(206.f)});
            page->addChild(m_quotaBigNumLbl, 6);
        }
        m_quotaSubLbl = ui::createStyledLabel("/ 20 peticiones hoy", false, 0.46f, pal.text2, ps(260.f));
        if (m_quotaSubLbl) {
            m_quotaSubLbl->setAnchorPoint({0.f, 0.5f});
            m_quotaSubLbl->setPosition({px(1176.f), py(210.f)});
            page->addChild(m_quotaSubLbl, 6);
        }

        auto usoTrackDn = CCDrawNode::create();
        ui::drawRoundedRect(
            usoTrackDn,
            px(1132.f), py(256.f), ps(352.f), ps(12.f), ps(6.f),
            rgba4F(pal.bgOuter, 1.f), 0.f, ccc4f(0, 0, 0, 0)
        );
        usoTrackDn->drawSegment(ccp(px(1132.f), py(304.f)), ccp(px(1484.f), py(304.f)), ps(0.8f), rgba4F(pal.line, 0.6f));
        usoTrackDn->drawSegment(ccp(px(1132.f), py(342.f)), ccp(px(1484.f), py(342.f)), ps(0.8f), rgba4F(pal.line, 0.6f));
        page->addChild(usoTrackDn, 4);

        m_quotaBarFill = CCDrawNode::create();
        page->addChild(m_quotaBarFill, 5);

        if (auto r1 = ui::createStyledLabel("Peticiones realizadas", false, 0.42f, pal.text2, ps(220.f))) {
            r1->setAnchorPoint({0.f, 0.5f});
            r1->setPosition({px(1132.f), py(285.f)});
            page->addChild(r1, 5);
        }
        m_reqDoneValLbl = ui::createStyledLabel("0", true, 0.26f, pal.text, ps(100.f));
        if (m_reqDoneValLbl) {
            m_reqDoneValLbl->setAnchorPoint({1.f, 0.5f});
            m_reqDoneValLbl->setPosition({px(1484.f), py(285.f)});
            page->addChild(m_reqDoneValLbl, 5);
        }

        if (auto r2 = ui::createStyledLabel("Nivel actual", false, 0.42f, pal.text2, ps(160.f))) {
            r2->setAnchorPoint({0.f, 0.5f});
            r2->setPosition({px(1132.f), py(323.f)});
            page->addChild(r2, 5);
        }
        m_curLevelValLbl = ui::createStyledLabel("Unnamed", true, 0.25f, pal.text, ps(210.f));
        if (m_curLevelValLbl) {
            m_curLevelValLbl->setAnchorPoint({1.f, 0.5f});
            m_curLevelValLbl->setPosition({px(1484.f), py(323.f)});
            page->addChild(m_curLevelValLbl, 5);
        }

        if (auto r3 = ui::createStyledLabel("Modo activo", false, 0.42f, pal.text2, ps(160.f))) {
            r3->setAnchorPoint({0.f, 0.5f});
            r3->setPosition({px(1132.f), py(366.f)});
            page->addChild(r3, 5);
        }
        ui::addRefRoundedBox(
            page, 1372.f, 349.f, 112.f, 34.f, 10.f,
            pal.elevated, 0.98f, pal.lineStrong, 0.90f, 1.0f, 4
        );
        m_activeModeValLbl = ui::createStyledLabel("Estructurar", false, 0.40f, pal.accentSoft, ps(98.f));
        if (m_activeModeValLbl) {
            m_activeModeValLbl->setPosition({px(1428.f), py(366.f)});
            page->addChild(m_activeModeValLbl, 5);
        }

        // Panel 2: Marco exterior de CHAT / COMANDO en Pagina 0 (y_top_ref = 412..668)
        ui::addRefRoundedBox(
            page, 1108.f, 412.f, 400.f, 256.f, 18.f,
            pal.panel, 0.98f, pal.line, 0.90f, 1.4f, 3
        );
        if (auto cTitle = ui::createStyledLabel("CHAT / COMANDO", true, 0.23f, pal.text2, ps(240.f))) {
            cTitle->setAnchorPoint({0.f, 0.5f});
            cTitle->setPosition({px(1132.f), py(440.f)});
            page->addChild(cTitle, 5);
        }

        // Panel 3: HISTORIAL en Pagina 0 (y_top_ref = 682..880)
        this->buildMiniHistoryCard(page, 1108.f, 682.f, 400.f, 198.f, m_histRowText, m_histRowTime);
    }

    void buildMiniHistoryCard(
        CCNode* parent,
        float rx, float ry_top, float rw, float rh,
        CCLabelBMFont* outTexts[3],
        CCLabelBMFont* outTimes[3]
    ) {
        auto pal = ThemeConfig::getPalette();
        ui::addRefRoundedBox(
            parent, rx, ry_top, rw, rh, 18.f,
            pal.panel, 0.98f, pal.line, 0.90f, 1.4f, 3
        );

        if (auto hTitle = ui::createStyledLabel("HISTORIAL", true, 0.23f, pal.text2, ps(180.f))) {
            hTitle->setAnchorPoint({0.f, 0.5f});
            hTitle->setPosition({px(rx + 24.f), py(ry_top + 28.f)});
            parent->addChild(hTitle, 5);
        }

        auto hMenu = CCMenu::create();
        hMenu->setPosition({0.f, 0.f});
        parent->addChild(hMenu, 10);

        auto verTodoNode = CCNode::create();
        verTodoNode->setContentSize({ps(92.f), ps(26.f)});
        if (auto vtLbl = ui::createStyledLabel("Ver todo >", false, 0.40f, pal.accentSoft, ps(90.f))) {
            vtLbl->setPosition({ps(46.f), ps(13.f)});
            verTodoNode->addChild(vtLbl);
        }
        auto vtBtn = CCMenuItemExt::createSpriteExtra(
            verTodoNode, [this](CCMenuItemSpriteExtra*) { this->openFullHistoryModal(); }
        );
        vtBtn->m_scaleMultiplier = 1.03f;
        vtBtn->setPosition({px(rx + rw - 64.f), py(ry_top + 28.f)});
        hMenu->addChild(vtBtn);

        auto hDn = CCDrawNode::create();
        float rowStep = (rh - 54.f) / 3.f;
        for (int r = 0; r < 3; ++r) {
            float rowY = ry_top + 68.f + r * rowStep;
            outTexts[r] = ui::createStyledLabel("", false, 0.36f, pal.text, ps(rw - 150.f));
            if (outTexts[r]) {
                outTexts[r]->setAnchorPoint({0.f, 0.5f});
                outTexts[r]->setPosition({px(rx + 46.f), py(rowY)});
                parent->addChild(outTexts[r], 5);
            }

            outTimes[r] = ui::createStyledLabel("", false, 0.37f, pal.text3, ps(76.f));
            if (outTimes[r]) {
                outTimes[r]->setAnchorPoint({1.f, 0.5f});
                outTimes[r]->setPosition({px(rx + rw - 22.f), py(rowY)});
                parent->addChild(outTimes[r], 5);
            }
        }
        parent->addChild(hDn, 4);
    }

    void updateInicioScrollbar() {
        if (!m_inicioScrollBarNode || !m_inicioScroll || !m_inicioScroll->m_contentLayer) return;
        m_inicioScrollBarNode->clear();
        auto pal = ThemeConfig::getPalette();

        float trackX = px(1092.f);
        float trackTopY = py(184.f);
        float trackBotY = py(866.f);
        float trackH = trackTopY - trackBotY;
        float trackW = ps(5.f);

        ui::drawRoundedRect(
            m_inicioScrollBarNode,
            trackX, trackBotY, trackW, trackH, ps(2.5f),
            rgba4F(pal.panel, 0.75f), 0.f, ccc4f(0, 0, 0, 0)
        );

        float curY = m_inicioScroll->m_contentLayer->getPositionY();
        float range = std::max(1.f, -m_inicioMinScrollY);
        float scrollFrac = std::clamp((curY - m_inicioMinScrollY) / range, 0.f, 1.f);
        float thumbH = trackH * 0.65f;
        float thumbY = trackTopY - thumbH - scrollFrac * (trackH - thumbH);

        ui::drawRoundedRect(
            m_inicioScrollBarNode,
            trackX, thumbY, trackW, thumbH, ps(2.5f),
            rgba4F(pal.accent, 0.88f), 0.f, ccc4f(0, 0, 0, 0)
        );
    }

    // -----------------------------------------------------------------------
    // 4. PAGINA 1 (INFORMACION / MODOS - Pagina 2 del PDF):
    //    Lista izquierda de los 9 modos + Panel derecho de Detalle del Modo
    // -----------------------------------------------------------------------
    void buildPage1Informacion() {
        auto page = m_centerPages[1];
        auto pal = ThemeConfig::getPalette();

        // Columna izquierda: Lista de los 9 modos (x_ref = 254..668, y_top_ref = 128..880)
        ui::addRefRoundedBox(
            page, 254.f, 128.f, 414.f, 752.f, 18.f,
            pal.panel, 0.98f, pal.line, 0.90f, 1.4f, 2
        );

        float listW = ps(386.f);
        float listH = ps(724.f);
        m_infoListScroll = ScrollLayer::create(CCSize{listW, listH});
        m_infoListScroll->ignoreAnchorPointForPosition(false);
        m_infoListScroll->setAnchorPoint({0.5f, 0.5f});
        m_infoListScroll->setPosition({px(461.f), py(504.f)});
        page->addChild(m_infoListScroll, 5);

        auto content = m_infoListScroll->m_contentLayer;
        const float rowH = ps(68.f);
        const float rowGap = ps(12.f);
        float totalH = std::max(listH, ps(12.f) + 9.f * rowH + 8.f * rowGap);
        content->setContentSize({listW, totalH});
        m_infoMinScrollY = listH - totalH;

        for (int i = 0; i < 9; ++i) {
            float ry = totalH - ps(6.f) - rowH - i * (rowH + rowGap);
            auto const& m = MODES[i];
            bool isSel = (i == m_selectedMode);

            auto rowNode = CCNode::create();
            rowNode->setContentSize({listW, rowH});
            rowNode->ignoreAnchorPointForPosition(false);
            rowNode->setAnchorPoint({0.5f, 0.5f});
            rowNode->setPosition({listW * 0.5f, ry + rowH * 0.5f});

            auto dn = CCDrawNode::create();
            dn->setTag(790);
            ui::drawRoundedRect(
                dn, 0.f, 0.f, listW, rowH, ps(14.f),
                rgba4F(isSel ? pal.elevated : pal.card, 0.98f),
                ps(isSel ? 1.8f : 1.0f),
                rgba4F(isSel ? pal.accent : pal.line, isSel ? 0.96f : 0.82f)
            );
            rowNode->addChild(dn, 1);

            auto badge = ui::createModeIconBadge(i, isSel, 42.f, 11.f);
            badge->setTag(791);
            badge->ignoreAnchorPointForPosition(false);
            badge->setAnchorPoint({0.5f, 0.5f});
            badge->setPosition({ps(34.f), rowH * 0.5f});
            rowNode->addChild(badge, 3);

            if (auto tLbl = ui::createStyledLabel(m.title, true, 0.23f, pal.text, ps(220.f))) {
                tLbl->setAnchorPoint({0.f, 0.5f});
                tLbl->setPosition({ps(68.f), rowH * 0.5f});
                rowNode->addChild(tLbl, 4);
            }

            if (isSel) {
                auto tagDn = CCDrawNode::create();
                tagDn->setTag(792);
                ui::drawRoundedRect(
                    tagDn, listW - ps(88.f), rowH * 0.5f - ps(14.f), ps(72.f), ps(28.f), ps(8.f),
                    rgba4F(pal.card, 1.f), ps(1.0f), rgba4F(pal.lineStrong, 0.9f)
                );
                rowNode->addChild(tagDn, 3);
            }
            if (auto catLbl = ui::createStyledLabel(m.categoryTag, false, 0.39f, isSel ? pal.accentSoft : pal.text3, ps(66.f))) {
                catLbl->setAnchorPoint({isSel ? 0.5f : 1.f, 0.5f});
                catLbl->setPosition({isSel ? (listW - ps(52.f)) : (listW - ps(18.f)), rowH * 0.5f});
                rowNode->addChild(catLbl, 4);
            }

            content->addChild(rowNode, 5);
            m_infoRowNodes[i] = rowNode;
        }
        if (m_savedInfoScrollY <= 0.f && m_savedInfoScrollY >= m_infoMinScrollY) {
            content->setPositionY(m_savedInfoScrollY);
        } else {
            m_infoListScroll->scrollToTop();
        }

        // Columna derecha: Panel de Detalle del Modo (x_ref = 684..1508, y_top_ref = 128..880)
        ui::addRefRoundedBox(
            page, 684.f, 128.f, 824.f, 752.f, 18.f,
            pal.panel, 0.98f, pal.line, 0.90f, 1.4f, 2
        );

        m_infoDetailContainer = CCNode::create();
        page->addChild(m_infoDetailContainer, 6);
        this->refreshPage1Detail();
    }

    void refreshPage1Detail() {
        if (!m_infoDetailContainer) return;
        m_infoDetailContainer->removeAllChildren();

        auto pal = ThemeConfig::getPalette();
        int idx = (m_selectedMode >= 0 && m_selectedMode < 9) ? m_selectedMode : 0;
        auto const& m = MODES[idx];

        // 1. Icono grande 88x88 ref en color Acento arriba a la izquierda
        auto bigBadge = ui::createModeIconBadge(idx, true, 88.f, 20.f);
        bigBadge->ignoreAnchorPointForPosition(false);
        bigBadge->setAnchorPoint({0.f, 1.f});
        bigBadge->setPosition({px(724.f), py(168.f)});
        m_infoDetailContainer->addChild(bigBadge, 4);

        if (auto overLbl = ui::createStyledLabel("DETALLES DEL MODO", true, 0.20f, pal.text3, ps(300.f))) {
            overLbl->setAnchorPoint({0.f, 0.5f});
            overLbl->setPosition({px(832.f), py(180.f)});
            m_infoDetailContainer->addChild(overLbl, 4);
        }
        if (auto nameLbl = ui::createStyledLabel(m.name, true, 0.48f, pal.text, ps(580.f))) {
            nameLbl->setAnchorPoint({0.f, 0.5f});
            nameLbl->setPosition({px(832.f), py(212.f)});
            m_infoDetailContainer->addChild(nameLbl, 4);
        }
        std::string subStr = std::string(m.visibleLine1) + " " + m.visibleLine2;
        if (auto subLbl = ui::createStyledLabel(subStr, false, 0.46f, pal.text2, ps(620.f))) {
            subLbl->setAnchorPoint({0.f, 0.5f});
            subLbl->setPosition({px(832.f), py(248.f)});
            m_infoDetailContainer->addChild(subLbl, 4);
        }

        auto sepDn = CCDrawNode::create();
        sepDn->drawSegment(ccp(px(724.f), py(286.f)), ccp(px(1468.f), py(286.f)), ps(1.0f), rgba4F(pal.line, 0.75f));
        m_infoDetailContainer->addChild(sepDn, 2);

        // 2. QUE HACE ESTE MODO POR TU NIVEL
        if (auto qHdr = ui::createStyledLabel("QUE HACE ESTE MODO POR TU NIVEL", true, 0.22f, pal.text3, ps(500.f))) {
            qHdr->setAnchorPoint({0.f, 0.5f});
            qHdr->setPosition({px(724.f), py(320.f)});
            m_infoDetailContainer->addChild(qHdr, 4);
        }

        auto guideArea = SimpleTextArea::create(
            ThemeConfig::formatTextForActiveFont(m.userGuide),
            ThemeConfig::getActiveFontFile(false),
            0.40f * ThemeConfig::getActiveFontScaleMul(false),
            ps(740.f)
        );
        guideArea->setWrappingMode(WORD_WRAP);
        guideArea->setColor(rgba4B(pal.text, 255));
        guideArea->setAnchorPoint({0.f, 1.f});
        guideArea->setPosition({px(724.f), py(348.f)});
        m_infoDetailContainer->addChild(guideArea, 4);

        // 3. PUEDE USAR (3 pildoras con ✓ y 2 pildoras con ✕ como en la Pagina 2 del PDF)
        if (auto puHdr = ui::createStyledLabel("PUEDE USAR", true, 0.22f, pal.text3, ps(300.f))) {
            puHdr->setAnchorPoint({0.f, 0.5f});
            puHdr->setPosition({px(724.f), py(442.f)});
            m_infoDetailContainer->addChild(puHdr, 4);
        }

        float curX = 724.f;
        for (int c = 0; c < 3; ++c) {
            std::string labelStr = std::string("+ ") + m.canUse[c];
            float pillW = std::min(244.f, 64.f + static_cast<float>(labelStr.size()) * 7.8f);
            ui::addRefRoundedBox(
                m_infoDetailContainer, curX, 466.f, pillW, 42.f, 12.f,
                pal.elevated, 0.98f, pal.lineStrong, 0.90f, 1.2f, 3
            );
            if (auto chk = ui::createFitSprite("ui-check.png"_spr, ps(14.f), ps(14.f), ccp(px(curX + 20.f), py(487.f)))) {
                chk->setColor(pal.accentSoft);
                m_infoDetailContainer->addChild(chk, 5);
            }
            if (auto pLbl = ui::createStyledLabel(m.canUse[c], false, 0.40f, pal.text, ps(pillW - 42.f))) {
                pLbl->setAnchorPoint({0.f, 0.5f});
                pLbl->setPosition({px(curX + 34.f), py(487.f)});
                m_infoDetailContainer->addChild(pLbl, 5);
            }
            curX += pillW + 14.f;
        }

        curX = 724.f;
        for (int c = 0; c < 2; ++c) {
            std::string labelStr = std::string("x ") + m.cannotUse[c];
            float pillW = std::min(280.f, 64.f + static_cast<float>(labelStr.size()) * 7.8f);
            ui::addRefRoundedBox(
                m_infoDetailContainer, curX, 522.f, pillW, 40.f, 12.f,
                pal.card, 0.96f, pal.line, 0.80f, 1.0f, 3
            );
            if (auto crs = ui::createFitSprite("ui-cross.png"_spr, ps(12.f), ps(12.f), ccp(px(curX + 20.f), py(542.f)))) {
                crs->setColor(pal.text3);
                m_infoDetailContainer->addChild(crs, 5);
            }
            if (auto pLbl = ui::createStyledLabel(m.cannotUse[c], false, 0.39f, pal.text3, ps(pillW - 42.f))) {
                pLbl->setAnchorPoint({0.f, 0.5f});
                pLbl->setPosition({px(curX + 34.f), py(542.f)});
                m_infoDetailContainer->addChild(pLbl, 5);
            }
            curX += pillW + 14.f;
        }

        // 4. EJEMPLO DE PRUEBA
        if (auto exHdr = ui::createStyledLabel("EJEMPLO DE PRUEBA", true, 0.22f, pal.text3, ps(300.f))) {
            exHdr->setAnchorPoint({0.f, 0.5f});
            exHdr->setPosition({px(724.f), py(602.f)});
            m_infoDetailContainer->addChild(exHdr, 4);
        }
        ui::addRefRoundedBox(
            m_infoDetailContainer, 724.f, 626.f, 740.f, 74.f, 14.f,
            pal.bgOuter, 0.98f, pal.line, 0.90f, 1.2f, 3
        );
        if (auto exLbl = ui::createStyledLabel(std::string("\"") + m.examplePrompt + "\"", false, 0.45f, pal.text, ps(700.f))) {
            exLbl->setAnchorPoint({0.f, 0.5f});
            exLbl->setPosition({px(746.f), py(663.f)});
            m_infoDetailContainer->addChild(exLbl, 5);
        }

        // 5. Botones inferiores: USAR ESTE MODO y PROBAR EL EJEMPLO
        auto detailMenu = CCMenu::create();
        detailMenu->setPosition({0.f, 0.f});
        m_infoDetailContainer->addChild(detailMenu, 10);

        auto usarSpr = ui::makeCustomMenuBtn("USAR ESTE MODO", ps(216.f), ps(54.f), pal.elevated, pal.lineStrong, 0.25f, false, ps(14.f));
        auto usarBtn = CCMenuItemExt::createSpriteExtra(usarSpr, [this, idx](CCMenuItemSpriteExtra*) {
            this->selectMode(idx);
            this->showPage(2);
        });
        usarBtn->m_scaleMultiplier = 1.03f;
        usarBtn->setPosition({px(832.f), py(798.f)});
        detailMenu->addChild(usarBtn);

        auto probarSpr = ui::makeCustomMenuBtn("PROBAR EL EJEMPLO", ps(248.f), ps(54.f), pal.elevated, pal.lineStrong, 0.25f, false, ps(14.f));
        auto probarBtn = CCMenuItemExt::createSpriteExtra(probarSpr, [this, idx](CCMenuItemSpriteExtra*) {
            this->selectMode(idx);
            this->setPromptText(MODES[idx].examplePrompt);
            this->showPage(2);
        });
        probarBtn->m_scaleMultiplier = 1.03f;
        probarBtn->setPosition({px(1082.f), py(798.f)});
        detailMenu->addChild(probarBtn);
    }

    // -----------------------------------------------------------------------
    // 5. PAGINA 2 (CHAT - Pagina 3 del PDF):
    //    Panel central de conversacion + Columna derecha (LO QUE VE LA IA, GRUPOS Y COLORES LIBRES, HISTORIAL)
    // -----------------------------------------------------------------------
    void buildPage2Chat() {
        auto page = m_centerPages[2];
        auto pal = ThemeConfig::getPalette();

        // Panel central amplio de Chat (x_ref = 254..1136, y_top_ref = 128..880)
        ui::addRefRoundedBox(
            page, 254.f, 128.f, 882.f, 752.f, 18.f,
            pal.panel, 0.98f, pal.line, 0.90f, 1.4f, 2
        );

        auto chatTopDn = CCDrawNode::create();
        chatTopDn->drawSegment(ccp(px(278.f), py(198.f)), ccp(px(1112.f), py(198.f)), ps(1.0f), rgba4F(pal.line, 0.75f));
        chatTopDn->drawSegment(ccp(px(278.f), py(664.f)), ccp(px(1112.f), py(664.f)), ps(1.0f), rgba4F(pal.line, 0.75f));
        page->addChild(chatTopDn, 3);

        if (auto mLbl = ui::createStyledLabel("Modo", false, 0.42f, pal.text3, ps(70.f))) {
            mLbl->setAnchorPoint({0.f, 0.5f});
            mLbl->setPosition({px(282.f), py(164.f)});
            page->addChild(mLbl, 5);
        }

        auto pageMenu = CCMenu::create();
        pageMenu->setPosition({0.f, 0.f});
        page->addChild(pageMenu, 12);

        // Nombre del modo: indicador informativo; se elige en Inicio o Informacion.
        int safeM = (m_selectedMode >= 0 && m_selectedMode < 9) ? m_selectedMode : 0;
        m_chatModeBtnLbl = ui::createStyledLabel(MODES[safeM].name, false, 0.41f, pal.accentSoft, ps(180.f));
        if (m_chatModeBtnLbl) {
            m_chatModeBtnLbl->setAnchorPoint({0.f, 0.5f});
            m_chatModeBtnLbl->setPosition({px(340.f), py(164.f)});
            page->addChild(m_chatModeBtnLbl, 5);
        }
        m_chatStateBadgeDn = nullptr;
        m_chatStateBadgeLbl = nullptr;
        m_chatUserBubbleLbl = nullptr;
        m_chatAiBubbleLbl = nullptr;
        m_chatScroll = ScrollLayer::create(CCSize{ps(834.f), ps(370.f)});
        m_chatScroll->ignoreAnchorPointForPosition(false);
        m_chatScroll->setAnchorPoint({0.5f, 0.5f});
        m_chatScroll->setPosition({px(695.f), py(404.f)});
        page->addChild(m_chatScroll, 5);
        this->refreshConversation();

        // 3 Pildoras de resumen debajo de la burbuja de la IA (fiel a Pagina 3 del PDF)
        const float pillX[3] = {278.f, 418.f, 564.f};
        const float pillW[3] = {128.f, 134.f, 128.f};
        char const* initPills[3] = {
            m_lastSummaryPill1.c_str(),
            m_lastSummaryPill2.c_str(),
            m_lastSummaryPill3.c_str()
        };
        for (int k = 0; k < 3; ++k) {
            // Las etiquetas de resumen pertenecen a cada mensaje, no al panel fijo.
            m_chatPillLbls[k] = ui::createStyledLabel(initPills[k], false, 0.37f, pal.text2, ps(pillW[k] - 16.f));
            if (m_chatPillLbls[k]) {
                m_chatPillLbls[k]->setVisible(false);
            }
        }

        ui::addRefRoundedBox(page, 278.f, 598.f, 834.f, 60.f, 10.f,
            pal.panel, .98f, pal.line, .9f, 1.f, 3);
        // Acciones fuera del viewport de mensajes (termina en y=589).
        auto undoSpr = ui::makeCustomMenuBtn("DESHACER", ps(118.f), ps(40.f), pal.card, pal.lineStrong, 0.22f, false, ps(10.f));
        auto undoBtn = CCMenuItemExt::createSpriteExtra(undoSpr, [this](CCMenuItemSpriteExtra*) {
            if (!m_canUndoAiAction) {
                this->updateChatStatusText("AVISO", COL_STATUS_AMBER, "Aun no hay una accion de la IA para deshacer.");
                return;
            }
            if (auto ui = EditorUI::get()) {
                ui->undoLastAction(nullptr);
                m_canUndoAiAction = false;
                this->updateChatStatusText("DESHECHO", COL_STATUS_AMBER, "Se deshizo la ultima accion en el editor.");
            }
        });
        undoBtn->m_scaleMultiplier = 1.03f;
        undoBtn->setPosition({px(337.f), py(628.f)});
        pageMenu->addChild(undoBtn);

        auto favSpr = ui::makeCustomMenuBtn("GUARDAR EN FAVORITOS", ps(216.f), ps(40.f), pal.card, pal.lineStrong, 0.22f, false, ps(10.f));
        auto favBtn = CCMenuItemExt::createSpriteExtra(favSpr, [this](CCMenuItemSpriteExtra*) {
            std::string curModeId = (m_selectedMode >= 0) ? MODES[m_selectedMode].id : "deco";
            if (m_lastUserBubbleText.empty()) {
                this->updateChatStatusText("AVISO", COL_STATUS_AMBER, "Envia primero un mensaje para guardarlo en Favoritos.");
                return;
            }
            std::string pToSave = m_lastUserBubbleText;
            HistoryManager::addFavorite(curModeId, pToSave);
            this->updateChatStatusText("FAVORITO GUARDADO", COL_STATUS_GREEN, "Comando guardado en tu lista de Favoritos.");
        });
        favBtn->m_scaleMultiplier = 1.03f;
        favBtn->setPosition({px(518.f), py(628.f)});
        pageMenu->addChild(favBtn);

        // Fila de 4 chips rapidos encima de la caja de texto (y_top_ref = 696)
        struct QuickChip { char const* label; char const* promptText; float x; float w; };
        QuickChip chips[4] = {
            {"/catalogo",                     "/catalogo",                                                    278.f, 102.f},
            {"Usar seleccion",                "Decora y estructura los objetos que tengo seleccionados",      390.f, 134.f},
            {"Tres pinchos seguidos",         "Genera una estructura pequena con un salto de tres pinchos",   534.f, 178.f},
            {"Deco neon para esta estructura","Mejora la deco de esta estructura con estilo neon brillante",  722.f, 236.f}
        };
        for (int c = 0; c < 4; ++c) {
            auto chipNode = CCNode::create();
            chipNode->setContentSize({ps(chips[c].w), ps(32.f)});
            auto cDn = CCDrawNode::create();
            ui::drawRoundedRect(
                cDn, 0.f, 0.f, ps(chips[c].w), ps(32.f), ps(10.f),
                rgba4F(pal.card, 0.96f), ps(1.0f), rgba4F(pal.line, 0.85f)
            );
            chipNode->addChild(cDn, 1);
            if (auto cLbl = ui::createStyledLabel(chips[c].label, false, 0.37f, pal.text2, ps(chips[c].w - 16.f))) {
                cLbl->setPosition({ps(chips[c].w * 0.5f), ps(16.f)});
                chipNode->addChild(cLbl, 2);
            }
            char const* pText = chips[c].promptText;
            auto chipBtn = CCMenuItemExt::createSpriteExtra(chipNode, [this, pText](CCMenuItemSpriteExtra*) {
                this->setPromptText(pText);
            });
            chipBtn->m_scaleMultiplier = 1.03f;
            chipBtn->setPosition({px(chips[c].x + chips[c].w * 0.5f), py(694.f)});
            pageMenu->addChild(chipBtn);
        }

        // El campo de entrada conserva un pie despejado.

        // --- COLUMNA DERECHA DE PAGINA 2 (x_ref = 1150..1508) ---
        // Card 1: LO QUE VE LA IA (y_top_ref = 128..366)
        ui::addRefRoundedBox(
            page, 1150.f, 128.f, 358.f, 238.f, 18.f,
            pal.panel, 0.98f, pal.line, 0.90f, 1.4f, 2
        );
        if (auto lvTitle = ui::createStyledLabel("LO QUE VE LA IA", true, 0.22f, pal.text2, ps(260.f))) {
            lvTitle->setAnchorPoint({0.f, 0.5f});
            lvTitle->setPosition({px(1174.f), py(156.f)});
            page->addChild(lvTitle, 5);
        }

        char const* percKeys[4] = {"Nivel", "Objetos seleccionados", "Zona visible", "Catalogo"};
        const float percY[4] = {196.f, 238.f, 280.f, 322.f};
        auto percDn = CCDrawNode::create();
        for (int k = 0; k < 4; ++k) {
            if (k > 0) {
                percDn->drawSegment(
                    ccp(px(1174.f), py(percY[k] - 21.f)),
                    ccp(px(1484.f), py(percY[k] - 21.f)),
                    ps(0.8f), rgba4F(pal.line, 0.6f)
                );
            }
            if (auto kl = ui::createStyledLabel(percKeys[k], false, 0.40f, pal.text2, ps(180.f))) {
                kl->setAnchorPoint({0.f, 0.5f});
                kl->setPosition({px(1174.f), py(percY[k])});
                page->addChild(kl, 5);
            }
            auto vl = ui::createStyledLabel("-", true, 0.24f, pal.text, ps(150.f));
            if (vl) {
                vl->setAnchorPoint({1.f, 0.5f});
                vl->setPosition({px(1484.f), py(percY[k])});
                page->addChild(vl, 5);
            }
            if (k == 0) m_chatRightLevelLbl = vl;
            else if (k == 1) m_chatRightSelLbl = vl;
            else if (k == 2) m_chatRightZoneLbl = vl;
            else m_chatRightCatLbl = vl;
        }
        page->addChild(percDn, 3);

        // Card 2: GRUPOS Y COLORES LIBRES (y_top_ref = 380..616)
        ui::addRefRoundedBox(
            page, 1150.f, 380.f, 358.f, 236.f, 18.f,
            pal.panel, 0.98f, pal.line, 0.90f, 1.4f, 2
        );
        if (auto gcTitle = ui::createStyledLabel("GRUPOS Y COLORES LIBRES", true, 0.22f, pal.text2, ps(310.f))) {
            gcTitle->setAnchorPoint({0.f, 0.5f});
            gcTitle->setPosition({px(1174.f), py(408.f)});
            page->addChild(gcTitle, 5);
        }

        ui::addRefRoundedBox(
            page, 1172.f, 432.f, 150.f, 104.f, 14.f,
            pal.card, 0.98f, pal.line, 0.90f, 1.0f, 3
        );
        if (auto sgLbl = ui::createStyledLabel("Siguiente grupo", false, 0.37f, pal.text3, ps(130.f))) {
            sgLbl->setAnchorPoint({0.f, 0.5f});
            sgLbl->setPosition({px(1188.f), py(456.f)});
            page->addChild(sgLbl, 5);
        }
        m_chatRightNextGroupLbl = ui::createStyledLabel("G1", true, 0.44f, pal.accentSoft, ps(130.f));
        if (m_chatRightNextGroupLbl) {
            m_chatRightNextGroupLbl->setAnchorPoint({0.f, 0.5f});
            m_chatRightNextGroupLbl->setPosition({px(1188.f), py(498.f)});
            page->addChild(m_chatRightNextGroupLbl, 5);
        }

        ui::addRefRoundedBox(
            page, 1336.f, 432.f, 150.f, 104.f, 14.f,
            pal.card, 0.98f, pal.line, 0.90f, 1.0f, 3
        );
        if (auto scLbl = ui::createStyledLabel("Siguiente color", false, 0.37f, pal.text3, ps(130.f))) {
            scLbl->setAnchorPoint({0.f, 0.5f});
            scLbl->setPosition({px(1352.f), py(456.f)});
            page->addChild(scLbl, 5);
        }
        m_chatRightNextColorLbl = ui::createStyledLabel("C1", true, 0.44f, pal.accentSoft, ps(130.f));
        if (m_chatRightNextColorLbl) {
            m_chatRightNextColorLbl->setAnchorPoint({0.f, 0.5f});
            m_chatRightNextColorLbl->setPosition({px(1352.f), py(498.f)});
            page->addChild(m_chatRightNextColorLbl, 5);
        }

        if (auto gcCap1 = ui::createStyledLabel("Puedes pedir rangos concretos: \"usa los grupos del", false, 0.36f, pal.text3, ps(314.f))) {
            gcCap1->setAnchorPoint({0.f, 0.5f});
            gcCap1->setPosition({px(1174.f), py(564.f)});
            page->addChild(gcCap1, 5);
        }
        if (auto gcCap2 = ui::createStyledLabel("20 al 25\".", false, 0.36f, pal.text3, ps(314.f))) {
            gcCap2->setAnchorPoint({0.f, 0.5f});
            gcCap2->setPosition({px(1174.f), py(588.f)});
            page->addChild(gcCap2, 5);
        }

        // Card 3: HISTORIAL en Pagina 2 (y_top_ref = 630..880)
        this->buildMiniHistoryCard(page, 1150.f, 630.f, 358.f, 250.f, m_chatHistRowText, m_chatHistRowTime);
    }

    void refreshConversation() {
        if (!m_chatScroll || !m_chatScroll->m_contentLayer) return;
        auto layer = m_chatScroll->m_contentLayer;
        layer->removeAllChildren();
        auto pal = ThemeConfig::getPalette();
        auto const& messages = HistoryManager::chatMessages();
        float viewW = ps(834.f), viewH = ps(370.f);
        if (messages.empty()) {
            layer->setContentSize({viewW, viewH});
            if (auto lbl = ui::createStyledLabel("Escribe un mensaje para comenzar.", false, 0.42f, pal.text3, ps(700.f))) {
                lbl->setPosition({viewW * .5f, viewH * .5f});
                layer->addChild(lbl);
            }
            layer->setPositionY(0.f);
            return;
        }
        struct Row { std::string text; std::string time; bool user; float height; };
        std::vector<Row> rows;
        float totalH = ps(12.f);
        for (auto const& msg : messages) {
            std::string wrapped;
            std::string line;
            std::string word;
            auto flush = [&]() {
                if (word.empty()) return;
                if (!line.empty() && line.size() + word.size() + 1 > 55) {
                    if (!wrapped.empty()) wrapped += '\n';
                    wrapped += line;
                    line.clear();
                }
                if (!line.empty()) line += ' ';
                line += word;
                word.clear();
            };
            for (char ch : msg.text) { if (ch == ' ') flush(); else word += ch; }
            flush();
            if (!line.empty()) { if (!wrapped.empty()) wrapped += '\n'; wrapped += line; }
            int lines = 1 + static_cast<int>(std::count(wrapped.begin(), wrapped.end(), '\n'));
            char clock[16] = {0};
            if (auto tm = std::localtime(&msg.timestamp)) std::strftime(clock, sizeof(clock), "%H:%M", tm);
            float h = ps(std::max(62.f, 34.f + lines * 25.f));
            rows.push_back({wrapped, clock, msg.fromUser, h});
            totalH += h + ps(12.f);
        }
        totalH = std::max(viewH, totalH);
        layer->setContentSize({viewW, totalH});
        float y = totalH - ps(12.f);
        for (auto const& row : rows) {
            y -= row.height;
            float x = row.user ? ps(284.f) : ps(12.f);
            auto dn = CCDrawNode::create();
            // ScrollLayer culls children by their bounds: geometry must be local
            // and the draw node must expose the bubble's actual rectangle.
            dn->setContentSize({ps(538.f), row.height});
            dn->setPosition({x, y});
            ui::drawRoundedRect(dn, 0.f, 0.f, ps(538.f), row.height, ps(14.f),
                rgba4F(row.user ? pal.accent : pal.card, .98f), ps(1.f),
                rgba4F(row.user ? pal.accentSoft : pal.line, .8f));
            layer->addChild(dn, 1);
            if (auto label = ui::createStyledLabel(row.text, false, .43f,
                    row.user ? pal.inkOnAccent : pal.text, ps(502.f))) {
                label->setAnchorPoint({0.f, 1.f});
                label->setPosition({x + ps(16.f), y + row.height - ps(12.f)});
                layer->addChild(label, 2);
            }
            if (auto time = ui::createStyledLabel(row.time, false, .32f,
                    row.user ? pal.inkOnAccent : pal.text3, ps(80.f))) {
                time->setAnchorPoint({1.f, 0.f});
                time->setPosition({x + ps(522.f), y + ps(5.f)});
                layer->addChild(time, 2);
            }
            y -= ps(12.f);
        }
        layer->setPositionY(viewH - totalH);
    }

    void refreshPage2ChatPerception() {
        EditorContextSnapshot ctx = LevelExecutor::scanEditorContext(m_lel);
        if (m_chatRightLevelLbl) {
            fitBMFontLabel(m_chatRightLevelLbl, LevelExecutor::getLevelTitle(m_lel), ps(150.f), 0.24f, true);
        }
        if (m_chatRightSelLbl) {
            fitBMFontLabel(m_chatRightSelLbl, fmt::format("{}", ctx.selectedCount), ps(150.f), 0.24f, true);
        }
        if (m_chatRightZoneLbl) {
            fitBMFontLabel(
                m_chatRightZoneLbl,
                fmt::format("x {} - {}", static_cast<int>(ctx.viewMinX), static_cast<int>(ctx.viewMaxX)),
                ps(155.f), 0.22f, true
            );
        }
        if (m_chatRightCatLbl) {
            fitBMFontLabel(m_chatRightCatLbl, "1 412 objetos", ps(150.f), 0.22f, true);
        }
        if (m_chatRightNextGroupLbl) {
            fitBMFontLabel(m_chatRightNextGroupLbl, fmt::format("G{}", ctx.nextFreeGroup), ps(130.f), 0.44f, true);
        }
        if (m_chatRightNextColorLbl) {
            fitBMFontLabel(m_chatRightNextColorLbl, fmt::format("C{}", ctx.nextFreeColor), ps(130.f), 0.44f, true);
        }
    }

    // Widget de Entrada de Texto compartido entre Pagina 0 (columna derecha) y Pagina 2 (barra inferior central)
    void buildSharedChatInputWidget() {
        m_chatInputWidget = CCNode::create();
        m_mainLayer->addChild(m_chatInputWidget, 25);

        m_chatInputBgDn = CCDrawNode::create();
        m_chatInputWidget->addChild(m_chatInputBgDn, 1);

        m_placeholderNode = CCNode::create();
        m_chatInputWidget->addChild(m_placeholderNode, 3);

        for (int l = 0; l < 4; ++l) {
            m_typedPreviewLines[l] = CCLabelBMFont::create("", "chatFont.fnt");
            m_typedPreviewLines[l]->setAnchorPoint({0.f, 0.5f});
            m_chatInputWidget->addChild(m_typedPreviewLines[l], 4);
        }

        m_tildeOverlayNode = CCNode::create();
        m_tildeOverlayNode->setPosition({0.f, 0.f});
        m_chatInputWidget->addChild(m_tildeOverlayNode, 5);

        m_promptInput = TextInput::create(ps(336.f), " ", "chatFont.fnt");
        m_promptInput->setCommonFilter(CommonFilter::Any);
        m_promptInput->setMaxCharCount(160);
        if (m_promptInput->getBGSprite()) {
            m_promptInput->getBGSprite()->setOpacity(0);
        }
        if (auto* inNode = m_promptInput->getInputNode()) {
            if (auto children = inNode->getChildren()) {
                for (unsigned int i = 0; i < children->count(); ++i) {
                    auto obj = children->objectAtIndex(i);
                    if (auto* tf = typeinfo_cast<cocos2d::CCTextFieldTTF*>(obj)) {
                        m_promptFieldDelegate.bind(
                            m_promptInput,
                            inNode,
                            tf,
                            static_cast<cocos2d::CCTextFieldDelegate*>(inNode),
                            [this](std::string const& updated) {
                                this->onPromptTextChanged(updated);
                            }
                        );
                    } else if (auto* bm = typeinfo_cast<CCLabelBMFont*>(obj)) {
                        bm->setOpacity(0);
                        bm->setScale(0.0001f);
                    }
                }
            }
        }
        m_promptInput->setCallback([this](std::string const& text) {
            this->onPromptTextChanged(text);
        });
        m_chatInputWidget->addChild(m_promptInput, 8);

        m_charCountLbl = ui::createStyledLabel("0 / 160 · prueba /catalogo", false, 0.37f, ThemeConfig::getPalette().text3, ps(260.f));
        if (m_charCountLbl) {
            m_charCountLbl->setAnchorPoint({0.f, 0.5f});
            m_chatInputWidget->addChild(m_charCountLbl, 6);
        }

        auto chatMenu = CCMenu::create();
        chatMenu->setPosition({0.f, 0.f});
        m_chatInputWidget->addChild(chatMenu, 15);

        auto pal = ThemeConfig::getPalette();
        float sendS = ps(52.f);
        auto sendNode = CCNode::create();
        sendNode->setContentSize({sendS, sendS});
        auto sendDn = CCDrawNode::create();
        ui::drawRoundedRect(
            sendDn, 0.f, 0.f, sendS, sendS, ps(12.f),
            rgba4F(pal.accent, 1.f), 0.f, ccc4f(0, 0, 0, 0)
        );
        sendNode->addChild(sendDn, 1);
        if (auto planeSpr = ui::createFitSprite("ui-send.png"_spr, ps(24.f), ps(24.f), ccp(sendS * 0.5f, sendS * 0.5f))) {
            planeSpr->setColor(pal.inkOnAccent);
            sendNode->addChild(planeSpr, 2);
        }

        m_sendBtnItem = CCMenuItemExt::createSpriteExtra(
            sendNode, [this](CCMenuItemSpriteExtra*) { this->onSendCommand(); }
        );
        m_sendBtnItem->m_scaleMultiplier = 1.04f;
        chatMenu->addChild(m_sendBtnItem);

        this->relayoutChatInputWidget(m_activePage == 2);
    }

    void relayoutChatInputWidget(bool wideChatMode) {
        if (!m_chatInputWidget || !m_chatInputBgDn) return;
        auto pal = ThemeConfig::getPalette();
        m_chatInputBgDn->clear();
        m_placeholderNode->removeAllChildren();

        float boxW = wideChatMode ? ps(730.f) : ps(352.f);
        float boxH = wideChatMode ? ps(104.f) : ps(130.f);
        float totalW = wideChatMode ? ps(810.f) : ps(352.f);
        float totalH = wideChatMode ? ps(140.f) : ps(196.f);

        m_chatInputWidget->setContentSize({totalW, totalH});
        m_chatInputWidget->ignoreAnchorPointForPosition(false);
        m_chatInputWidget->setAnchorPoint({0.5f, 0.5f});

        float boxX = 0.f;
        float boxY = wideChatMode ? ps(30.f) : ps(62.f);
        ui::drawRoundedRect(
            m_chatInputBgDn, boxX, boxY, boxW, boxH, ps(14.f),
            rgba4F(pal.bgOuter, 0.99f),
            ps(1.4f),
            rgba4F(wideChatMode ? pal.accent : pal.lineStrong, wideChatMode ? 0.85f : 0.90f)
        );

        if (wideChatMode) {
            if (auto pl = ui::createStyledLabel(
                "Haz un salto de tres pinchos con una plataforma pequena y decora el suelo con n",
                false, 0.42f, pal.text3, boxW - ps(32.f)
            )) {
                pl->setAnchorPoint({0.f, 0.5f});
                pl->setPosition({boxX + ps(16.f), boxY + boxH - ps(28.f)});
                m_placeholderNode->addChild(pl);
            }
        } else {
            char const* phLines[3] = {
                "Escribe lo que quieres que haga en tu nivel... (Ej.",
                "\"Haz una transicion entre 2 zonas\", \"Mejora la",
                "deco de esta parte\")"
            };
            for (int l = 0; l < 3; ++l) {
                if (auto pl = ui::createStyledLabel(phLines[l], false, 0.40f, pal.text3, boxW - ps(28.f))) {
                    pl->setAnchorPoint({0.f, 0.5f});
                    pl->setPosition({boxX + ps(14.f), boxY + boxH - ps(26.f + l * 26.f)});
                    m_placeholderNode->addChild(pl);
                }
            }
        }

        for (int l = 0; l < 4; ++l) {
            if (m_typedPreviewLines[l]) {
                m_typedPreviewLines[l]->setColor(pal.text);
                m_typedPreviewLines[l]->setPosition({
                    boxX + ps(16.f),
                    boxY + boxH - ps(24.f + l * 24.f)
                });
            }
        }

        if (m_promptInput) {
            m_promptInput->setPosition({boxX + boxW * 0.5f, boxY + boxH * 0.5f});
            m_promptInput->setContentSize({boxW, boxH});
            if (auto* inNode = m_promptInput->getInputNode()) {
                inNode->setContentSize({boxW, boxH});
            }
        }

        if (m_sendBtnItem) {
            if (wideChatMode) {
                m_sendBtnItem->setPosition({boxX + boxW + ps(40.f), boxY + ps(30.f)});
            } else {
                m_sendBtnItem->setPosition({boxX + boxW - ps(26.f), ps(26.f)});
            }
        }

        if (m_charCountLbl) {
            if (wideChatMode) {
                m_charCountLbl->setAnchorPoint({1.f, 0.5f});
                m_charCountLbl->setPosition({boxX + boxW, ps(10.f)});
            } else {
                m_charCountLbl->setAnchorPoint({0.f, 0.5f});
                m_charCountLbl->setPosition({boxX + ps(4.f), ps(26.f)});
            }
        }

        if (m_promptInput) {
            this->onPromptTextChanged(m_promptInput->getString());
        }
    }

    // -----------------------------------------------------------------------
    // 6. PAGINA 3 (ESTADISTICAS - Pagina 4 del PDF):
    //    4 tarjetas KPI + Grafico 7 dias + Modos mas usados + Puntos de guardado
    // -----------------------------------------------------------------------
    void buildPage3Estadisticas() {
        auto page = m_centerPages[3];
        m_statsContentHolder = CCNode::create();
        page->addChild(m_statsContentHolder, 5);
        this->refreshPage3Stats();
    }

    void refreshPage3Stats() {
        if (!m_statsContentHolder) return;
        m_statsContentHolder->removeAllChildren();

        auto pal = ThemeConfig::getPalette();
        int used = Mod::get()->getSavedValue<int>("used", 0);
        int limit = std::max(1, Mod::get()->getSavedValue<int>("limit", 20));
        int rem = std::max(0, limit - used);
        EditorContextSnapshot ctx = LevelExecutor::scanEditorContext(m_lel);

        // 1. Fila Superior: 4 Tarjetas KPI (x_ref = 254..1508, y_top_ref = 128..304)
        struct KpiCard {
            char const* title;
            std::string bigVal;
            std::string unitVal;
            std::string footer;
        };
        int usedGroupsCount = std::max(0, ctx.nextFreeGroup - 1);
        int usedColorsCount = std::max(0, ctx.nextFreeColor - 1);
        KpiCard kpis[4] = {
            {"PETICIONES HOY",    fmt::format("{}", used),                     fmt::format("/ {}", limit), "Quedan " + fmt::format("{} hasta medianoche", rem)},
            {"OBJETOS COLOCADOS", fmt::format("{}", m_sessionPlacedObjects),   "en esta sesion",           m_lastSummaryPill1.empty() ? "Sin acciones aun" : "Ultima: " + m_lastSummaryPill1},
            {"GRUPOS OCUPADOS",   fmt::format("{}", usedGroupsCount),          "de 9 999",                 fmt::format("Siguiente libre: G{}", ctx.nextFreeGroup)},
            {"COLORES OCUPADOS",  fmt::format("{}", usedColorsCount),          "de 999",                   fmt::format("Siguiente libre: C{}", ctx.nextFreeColor)}
        };

        const float kpiW = 301.f;
        const float kpiGap = 16.f;
        for (int i = 0; i < 4; ++i) {
            float kx = 254.f + i * (kpiW + kpiGap);
            ui::addRefRoundedBox(
                m_statsContentHolder, kx, 128.f, kpiW, 176.f, 18.f,
                pal.panel, 0.98f, pal.line, 0.90f, 1.4f, 2
            );
            if (auto kt = ui::createStyledLabel(kpis[i].title, true, 0.21f, pal.text2, ps(kpiW - 36.f))) {
                kt->setAnchorPoint({0.f, 0.5f});
                kt->setPosition({px(kx + 22.f), py(158.f)});
                m_statsContentHolder->addChild(kt, 4);
            }
            auto kb = ui::createStyledLabel(kpis[i].bigVal, true, 0.52f, pal.accentSoft, ps(120.f));
            float bigEndX = kx + 86.f;
            if (kb) {
                kb->setAnchorPoint({0.f, 0.5f});
                kb->setPosition({px(kx + 22.f), py(214.f)});
                m_statsContentHolder->addChild(kb, 4);
                bigEndX = kx + 30.f + (kb->getContentSize().width * kb->getScale()) / SCALE_K;
            }
            if (auto ku = ui::createStyledLabel(kpis[i].unitVal, false, 0.42f, pal.text2, ps(160.f))) {
                ku->setAnchorPoint({0.f, 0.5f});
                ku->setPosition({px(bigEndX), py(220.f)});
                m_statsContentHolder->addChild(ku, 4);
            }
            if (auto kf = ui::createStyledLabel(kpis[i].footer, false, 0.38f, pal.text3, ps(kpiW - 36.f))) {
                kf->setAnchorPoint({0.f, 0.5f});
                kf->setPosition({px(kx + 22.f), py(268.f)});
                m_statsContentHolder->addChild(kf, 4);
            }
        }

        // 2. Fila Media Izquierda: PETICIONES · ULTIMOS 7 DIAS (x_ref = 254..974, y_top_ref = 320..626)
        ui::addRefRoundedBox(
            m_statsContentHolder, 254.f, 320.f, 720.f, 306.f, 18.f,
            pal.panel, 0.98f, pal.line, 0.90f, 1.4f, 2
        );
        if (auto chTitle = ui::createStyledLabel("PETICIONES · ULTIMOS 7 DIAS", true, 0.22f, pal.text2, ps(420.f))) {
            chTitle->setAnchorPoint({0.f, 0.5f});
            chTitle->setPosition({px(278.f), py(350.f)});
            m_statsContentHolder->addChild(chTitle, 4);
        }
        if (auto chSub = ui::createStyledLabel(fmt::format("Limite: {} por dia", limit), false, 0.38f, pal.text3, ps(200.f))) {
            chSub->setAnchorPoint({1.f, 0.5f});
            chSub->setPosition({px(950.f), py(350.f)});
            m_statsContentHolder->addChild(chSub, 4);
        }

        char const* dayLetters[7] = {"L", "M", "X", "J", "V", "S", "D"};
        int barVals[7] = {};
        std::time_t now = std::time(nullptr);
        for (auto const& entry : HistoryManager::entries()) {
            if (entry.timestamp <= 0 || entry.timestamp > now) continue;
            int ago = static_cast<int>((now - entry.timestamp) / 86400);
            if (ago >= 0 && ago < 7) ++barVals[6 - ago];
        }
        auto chartDn = CCDrawNode::create();
        for (int d = 0; d < 7; ++d) {
            float bx = 286.f + d * 94.f;
            float bw = 72.f;
            float maxBarH = 160.f;
            float bh = barVals[d] == 0 ? 0.f : std::max(18.f, (static_cast<float>(barVals[d]) / std::max(12, limit)) * maxBarH);
            float by_bot = 576.f;
            ccColor3B barCol = (d == 6) ? pal.accent : pal.accentDark;
            if (bh > 0.f) ui::drawRoundedRect(
                chartDn,
                px(bx), py(by_bot), ps(bw), ps(bh), ps(8.f),
                rgba4F(barCol, 0.95f), 0.f, ccc4f(0, 0, 0, 0)
            );
            if (auto vLbl = ui::createStyledLabel(fmt::format("{}", barVals[d]), true, 0.21f, pal.text, ps(60.f))) {
                vLbl->setPosition({px(bx + bw * 0.5f), py(by_bot - bh - 16.f)});
                m_statsContentHolder->addChild(vLbl, 5);
            }
            if (auto dLbl = ui::createStyledLabel(dayLetters[d], false, 0.38f, pal.text3, ps(40.f))) {
                dLbl->setPosition({px(bx + bw * 0.5f), py(596.f)});
                m_statsContentHolder->addChild(dLbl, 5);
            }
        }
        m_statsContentHolder->addChild(chartDn, 3);

        // 3. Fila Media Derecha: MODOS MAS USADOS (x_ref = 990..1508, y_top_ref = 320..626)
        ui::addRefRoundedBox(
            m_statsContentHolder, 990.f, 320.f, 518.f, 306.f, 18.f,
            pal.panel, 0.98f, pal.line, 0.90f, 1.4f, 2
        );
        if (auto mmTitle = ui::createStyledLabel("MODOS MAS USADOS", true, 0.22f, pal.text2, ps(320.f))) {
            mmTitle->setAnchorPoint({0.f, 0.5f});
            mmTitle->setPosition({px(1014.f), py(350.f)});
            m_statsContentHolder->addChild(mmTitle, 4);
        }

        struct TopModeRow { char const* name; int modeIdx; int count; float frac; };
        int counts[9] = {};
        for (auto const& entry : HistoryManager::entries()) {
            int idx = modeIndexById(entry.modeId);
            if (idx >= 0) ++counts[idx];
        }
        std::vector<TopModeRow> topModes;
        for (int i = 0; i < 9; ++i) if (counts[i]) topModes.push_back({MODES[i].name, i, counts[i], 0.f});
        std::sort(topModes.begin(), topModes.end(), [](auto const& a, auto const& b) { return a.count > b.count; });
        if (topModes.empty()) {
            if (auto empty = ui::createStyledLabel("Sin peticiones registradas.", false, .4f, pal.text3, ps(400.f))) {
                empty->setPosition({px(1248.f), py(470.f)});
                m_statsContentHolder->addChild(empty, 4);
            }
        }
        auto mmDn = CCDrawNode::create();
        for (int m = 0; m < std::min(5, static_cast<int>(topModes.size())); ++m) {
            float ry = 398.f + m * 46.f;
            if (auto ml = ui::createStyledLabel(topModes[m].name, false, 0.40f, pal.text2, ps(126.f))) {
                ml->setAnchorPoint({0.f, 0.5f});
                ml->setPosition({px(1014.f), py(ry)});
                m_statsContentHolder->addChild(ml, 4);
            }
            float barW = 304.f * topModes[m].count / std::max(1, topModes[0].count);
            ui::drawRoundedRect(
                mmDn,
                px(1148.f), py(ry + 7.f), ps(barW), ps(14.f), ps(7.f),
                rgba4F(pal.modeAccent(topModes[m].modeIdx), 0.98f), 0.f, ccc4f(0, 0, 0, 0)
            );
            if (auto cl = ui::createStyledLabel(fmt::format("{}", topModes[m].count), true, 0.22f, pal.text, ps(40.f))) {
                cl->setAnchorPoint({1.f, 0.5f});
                cl->setPosition({px(1484.f), py(ry)});
                m_statsContentHolder->addChild(cl, 4);
            }
        }
        m_statsContentHolder->addChild(mmDn, 3);

        // 4. Fila Inferior: PUNTOS DE GUARDADO (x_ref = 254..1508, y_top_ref = 642..880)
        ui::addRefRoundedBox(
            m_statsContentHolder, 254.f, 642.f, 1254.f, 238.f, 18.f,
            pal.panel, 0.98f, pal.line, 0.90f, 1.4f, 2
        );
        if (auto pgTitle = ui::createStyledLabel("REGISTROS DEL NIVEL", true, 0.22f, pal.text2, ps(340.f))) {
            pgTitle->setAnchorPoint({0.f, 0.5f});
            pgTitle->setPosition({px(278.f), py(672.f)});
            m_statsContentHolder->addChild(pgTitle, 4);
        }

        auto stMenu = CCMenu::create();
        stMenu->setPosition({0.f, 0.f});
        m_statsContentHolder->addChild(stMenu, 10);

        auto verTodosNode = CCNode::create();
        verTodosNode->setContentSize({ps(100.f), ps(26.f)});
        if (auto vtLbl = ui::createStyledLabel("Ver todos >", false, 0.40f, pal.accentSoft, ps(96.f))) {
            vtLbl->setPosition({ps(50.f), ps(13.f)});
            verTodosNode->addChild(vtLbl);
        }
        auto vtBtn = CCMenuItemExt::createSpriteExtra(verTodosNode, [this](CCMenuItemSpriteExtra*) {
            if (auto pop = LevelsBackupPopup::create(m_lel)) pop->show();
        });
        vtBtn->m_scaleMultiplier = 1.03f;
        vtBtn->setPosition({px(1446.f), py(672.f)});
        stMenu->addChild(vtBtn);

        auto snaps = LevelExecutor::listSnapshots();
        struct SnapRowData { std::string name; std::string objs; std::string time; };
        SnapRowData rows[3] = {};
        for (size_t s = 0; s < std::min<size_t>(3, snaps.size()); ++s) {
            rows[s].name = snaps[s].levelName;
            rows[s].objs = fmt::format("{} objetos", snaps[s].objectCount);
            rows[s].time = snaps[s].dateStr;
        }

        auto pgDn = CCDrawNode::create();
        if (snaps.empty()) {
            if (auto empty = ui::createStyledLabel("Aun no hay puntos de guardado.", false, .4f, pal.text3, ps(600.f))) {
                empty->setPosition({px(620.f), py(750.f)});
                m_statsContentHolder->addChild(empty, 4);
            }
        }
        for (int r = 0; r < std::min(3, static_cast<int>(snaps.size())); ++r) {
            float ry = 720.f + r * 54.f;
            pgDn->drawSegment(
                ccp(px(278.f), py(ry - 27.f)),
                ccp(px(1484.f), py(ry - 27.f)),
                ps(0.8f), rgba4F(pal.line, 0.65f)
            );
            if (auto fSpr = ui::createFitSprite("ui-folder.png"_spr, ps(20.f), ps(18.f), ccp(px(290.f), py(ry)))) {
                fSpr->setColor(pal.accentSoft);
                m_statsContentHolder->addChild(fSpr, 4);
            }
            if (auto nl = ui::createStyledLabel(rows[r].name, true, 0.23f, pal.text, ps(540.f))) {
                nl->setAnchorPoint({0.f, 0.5f});
                nl->setPosition({px(314.f), py(ry)});
                m_statsContentHolder->addChild(nl, 4);
            }
            if (auto ol = ui::createStyledLabel(rows[r].objs, false, 0.40f, pal.text2, ps(160.f))) {
                ol->setAnchorPoint({1.f, 0.5f});
                ol->setPosition({px(1206.f), py(ry)});
                m_statsContentHolder->addChild(ol, 4);
            }
            if (auto tl = ui::createStyledLabel(rows[r].time, false, 0.39f, pal.text3, ps(140.f))) {
                tl->setAnchorPoint({1.f, 0.5f});
                tl->setPosition({px(1364.f), py(ry)});
                m_statsContentHolder->addChild(tl, 4);
            }
            if (auto info = ui::createStyledLabel("REGISTRO", false, .34f, pal.text3, ps(102.f))) {
                info->setPosition({px(1434.f), py(ry)});
                m_statsContentHolder->addChild(info, 4);
            }
        }
        m_statsContentHolder->addChild(pgDn, 3);
    }

    // -----------------------------------------------------------------------
    // 7. PAGINA 4 (AJUSTES - Pagina 5 del PDF + Selector de Fuentes + Rueda Oficial)
    // -----------------------------------------------------------------------
    void buildPage4Ajustes() {
        auto page = m_centerPages[4];
        auto pal = ThemeConfig::getPalette();

        // Columna central deslizable con todas las tarjetas de Ajustes (x_ref = 254..1068, y_top_ref = 128..880)
        float viewW = ps(814.f);
        float viewH = ps(752.f);
        m_settingsScroll = ScrollLayer::create(CCSize{viewW, viewH});
        m_settingsScroll->ignoreAnchorPointForPosition(false);
        m_settingsScroll->setAnchorPoint({0.5f, 0.5f});
        m_settingsScroll->setPosition({px(661.f), py(504.f)});
        page->addChild(m_settingsScroll, 5);

        auto content = m_settingsScroll->m_contentLayer;
        // Altura total para albergar holgadamente las 7 tarjetas de configuracion (incluyendo Fuente de letras)
        float totalH = ps(960.f);
        content->setContentSize({viewW, totalH});
        m_settingsMinScrollY = viewH - totalH;

        auto sMenu = CCMenu::create();
        sMenu->setPosition({0.f, 0.f});
        content->addChild(sMenu, 15);

        auto sDn = CCDrawNode::create();
        content->addChild(sDn, 2);

        auto yFromTop = [totalH](float y_ref) -> float {
            return totalH - ps(y_ref);
        };

        auto drawSettingCard = [&](float yTopRef, float hRef) {
            ui::drawRoundedRect(
                sDn, 0.f, yFromTop(yTopRef + hRef), viewW, ps(hRef), ps(16.f),
                rgba4F(pal.panel, 0.98f), ps(1.3f), rgba4F(pal.line, 0.90f)
            );
        };

        // --- TARJETA 1: Modo de conexion (yTop=0, h=96) ---
        drawSettingCard(0.f, 96.f);
        if (auto t1 = ui::createStyledLabel("Modo de conexion", true, 0.25f, pal.text, ps(420.f))) {
            t1->setAnchorPoint({0.f, 0.5f});
            t1->setPosition({ps(24.f), yFromTop(34.f)});
            content->addChild(t1, 5);
        }
        if (auto d1 = ui::createStyledLabel("Modo local usa el generador contextual; Modo AI usa tu servidor HTTP.", false, 0.39f, pal.text3, ps(500.f))) {
            d1->setAnchorPoint({0.f, 0.5f});
            d1->setPosition({ps(24.f), yFromTop(66.f)});
            content->addChild(d1, 5);
        }

        // Control segmentado doble: [ Modo local | Modo AI ]
        bool isLocal = (AIClient::getOriginMode() == "local");
        ui::drawRoundedRect(
            sDn, ps(542.f), yFromTop(74.f), ps(248.f), ps(52.f), ps(12.f),
            rgba4F(pal.bgOuter, 0.98f), ps(1.1f), rgba4F(pal.line, 0.9f)
        );
        auto locBtnSpr = ui::makeCustomMenuBtn(
            "Modo local", ps(116.f), ps(42.f),
            isLocal ? pal.accent : pal.bgOuter,
            isLocal ? pal.accent : pal.bgOuter,
            0.22f, isLocal, ps(10.f)
        );
        auto locBtn = CCMenuItemExt::createSpriteExtra(locBtnSpr, [this](CCMenuItemSpriteExtra*) {
            AIClient::setOriginMode("local");
            this->scheduleRebuild();
        });
        locBtn->m_scaleMultiplier = 1.02f;
        locBtn->setPosition({ps(605.f), yFromTop(48.f)});
        sMenu->addChild(locBtn);

        auto aiBtnSpr = ui::makeCustomMenuBtn(
            "Modo AI", ps(120.f), ps(42.f),
            !isLocal ? pal.accent : pal.bgOuter,
            !isLocal ? pal.accent : pal.bgOuter,
            0.22f, !isLocal, ps(10.f)
        );
        auto aiBtn = CCMenuItemExt::createSpriteExtra(aiBtnSpr, [this](CCMenuItemSpriteExtra*) {
            AIClient::setOriginMode("server");
            this->scheduleRebuild();
        });
        aiBtn->m_scaleMultiplier = 1.02f;
        aiBtn->setPosition({ps(725.f), yFromTop(48.f)});
        sMenu->addChild(aiBtn);

        // --- TARJETA 2: Direccion del servidor (yTop=110, h=124) ---
        drawSettingCard(110.f, 124.f);
        if (auto t2 = ui::createStyledLabel("Direccion del servidor", true, 0.25f, pal.text, ps(420.f))) {
            t2->setAnchorPoint({0.f, 0.5f});
            t2->setPosition({ps(24.f), yFromTop(140.f)});
            content->addChild(t2, 5);
        }
        // Caja oscura compacta y centrada verticalmente debajo del titulo (sin cubrir letras)
        ui::drawRoundedRect(
            sDn, ps(24.f), yFromTop(214.f), ps(592.f), ps(46.f), ps(12.f),
            rgba4F(pal.bgOuter, 0.99f), ps(1.2f), rgba4F(pal.line, 0.92f)
        );
        m_serverUrlInput = TextInput::create(ps(564.f) / 0.74f, "http://127.0.0.1:8765/generate", "chatFont.fnt");
        m_serverUrlInput->setFilter("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/.:-_");
        m_serverUrlInput->setMaxCharCount(96);
        m_serverUrlInput->setScale(0.74f);
        if (m_serverUrlInput->getBGSprite()) {
            m_serverUrlInput->getBGSprite()->setOpacity(0);
            m_serverUrlInput->getBGSprite()->setVisible(false);
        }
        m_serverUrlInput->setString(AIClient::getServerUrl());
        m_serverUrlInput->setCallback([](std::string const& str) {
            std::string clean = trimStr(str);
            if (!clean.empty()) AIClient::setServerUrl(clean);
        });
        m_serverUrlInput->setPosition({ps(320.f), yFromTop(191.f)});
        content->addChild(m_serverUrlInput, 12);

        auto saveUrlSpr = ui::makeCustomMenuBtn("GUARDAR", ps(158.f), ps(46.f), pal.accent, pal.accent, 0.23f, true, ps(12.f));
        auto saveUrlBtn = CCMenuItemExt::createSpriteExtra(saveUrlSpr, [this](CCMenuItemSpriteExtra*) {
            std::string val = m_serverUrlInput ? trimStr(std::string(m_serverUrlInput->getString().c_str())) : "";
            if (val.empty()) {
                val = "http://127.0.0.1:8765/generate";
                if (m_serverUrlInput) m_serverUrlInput->setString(val);
            }
            AIClient::setServerUrl(val);
            if (m_settingsStatusLbl) {
                fitBMFontLabel(m_settingsStatusLbl, "Direccion guardada: " + val, ps(760.f), 0.42f);
            }
        });
        saveUrlBtn->m_scaleMultiplier = 1.03f;
        saveUrlBtn->setPosition({ps(711.f), yFromTop(191.f)});
        sMenu->addChild(saveUrlBtn);

        // --- TARJETA 3: Color del menu ("Todo el menu se recolorea con una paleta de ese tono") (yTop=248, h=154) ---
        drawSettingCard(248.f, 154.f);
        if (auto t3 = ui::createStyledLabel("Color del menu", true, 0.25f, pal.text, ps(380.f))) {
            t3->setAnchorPoint({0.f, 0.5f});
            t3->setPosition({ps(24.f), yFromTop(278.f)});
            content->addChild(t3, 5);
        }
        if (auto d3 = ui::createStyledLabel("Todo el menu se recolorea con una paleta de ese tono.", false, 0.39f, pal.text3, ps(500.f))) {
            d3->setAnchorPoint({0.f, 0.5f});
            d3->setPosition({ps(24.f), yFromTop(308.f)});
            content->addChild(d3, 5);
        }

        // Muestra cuadrada + #HEX arriba a la derecha de la tarjeta 3
        ui::drawRoundedRect(
            sDn, ps(654.f), yFromTop(314.f), ps(38.f), ps(38.f), ps(9.f),
            rgba4F(pal.accent, 1.f), ps(1.2f), rgba4F(COL_TEXT_WHITE, 0.9f)
        );
        if (auto hexLbl3 = ui::createStyledLabel(colorToHexStr(pal.accent), true, 0.22f, pal.text, ps(96.f))) {
            hexLbl3->setAnchorPoint({1.f, 0.5f});
            hexLbl3->setPosition({ps(790.f), yFromTop(295.f)});
            content->addChild(hexLbl3, 5);
        }

        // 8 circulos de tonos predefinidos + boton RUEDA DE COLOR
        for (int pIdx = 0; pIdx < 8; ++pIdx) {
            float hPreset = PRESET_HUES[pIdx];
            ccColor3B cPreset = hslToRgb(hPreset, 92.f, 64.f);
            float cx = 44.f + pIdx * 54.f;
            float cy = 362.f;
            bool isCurHue = (std::fabs(pal.hueDeg - hPreset) < 8.f);

            auto cNode = CCNode::create();
            cNode->setContentSize({ps(42.f), ps(42.f)});
            auto cDn = CCDrawNode::create();
            if (isCurHue) {
                ui::drawCircleShape(
                    cDn, ccp(ps(21.f), ps(21.f)), ps(21.f),
                    rgba4F(COL_TEXT_WHITE, 1.f), 0.f, ccc4f(0, 0, 0, 0), 24
                );
            }
            ui::drawCircleShape(
                cDn, ccp(ps(21.f), ps(21.f)), ps(17.5f),
                rgba4F(cPreset, 1.f), 0.f, ccc4f(0, 0, 0, 0), 24
            );
            cNode->addChild(cDn);

            auto cBtn = CCMenuItemExt::createSpriteExtra(cNode, [this, cPreset](CCMenuItemSpriteExtra*) {
                ThemeConfig::setMenuAccent(cPreset);
                m_colorEditTarget = 0;
                m_embCurColor = cPreset;
                this->scheduleRebuild();
            });
            cBtn->m_scaleMultiplier = 1.06f;
            cBtn->setPosition({ps(cx), yFromTop(cy)});
            sMenu->addChild(cBtn);
        }

        // La rueda de color ya esta disponible en el panel derecho.

        // --- TARJETA 4: Flujo RGB en contornos (yTop=416, h=96) ---
        drawSettingCard(416.f, 96.f);
        if (auto t4 = ui::createStyledLabel("Flujo RGB en contornos", true, 0.25f, pal.text, ps(360.f))) {
            t4->setAnchorPoint({0.f, 0.5f});
            t4->setPosition({ps(24.f), yFromTop(448.f)});
            content->addChild(t4, 5);
        }
        if (auto d4 = ui::createStyledLabel("Estela de luz que recorre los bordes y los pinchos.", false, 0.39f, pal.text3, ps(390.f))) {
            d4->setAnchorPoint({0.f, 0.5f});
            d4->setPosition({ps(24.f), yFromTop(480.f)});
            content->addChild(d4, 5);
        }

        if (auto vLbl = ui::createStyledLabel("Velocidad", false, 0.38f, pal.text3, ps(90.f))) {
            vLbl->setAnchorPoint({1.f, 0.5f});
            vLbl->setPosition({ps(616.f), yFromTop(438.f)});
            content->addChild(vLbl, 5);
        }

        // Barra deslizante e interactiva de velocidad (25%..300%) + interruptor
        int spdPct = ThemeConfig::getRgbSpeedPct();
        float spdFrac = std::clamp((spdPct - 25.f) / 275.f, 0.f, 1.f);
        ui::drawRoundedRect(
            sDn, ps(480.f), yFromTop(469.f), ps(136.f), ps(10.f), ps(5.f),
            rgba4F(pal.bgOuter, 1.f), 0.f, ccc4f(0, 0, 0, 0)
        );
        ui::drawRoundedRect(
            sDn, ps(480.f), yFromTop(469.f), ps(std::max(12.f, 136.f * spdFrac)), ps(10.f), ps(5.f),
            rgba4F(pal.accent, 1.f), 0.f, ccc4f(0, 0, 0, 0)
        );
        sDn->drawDot(ccp(ps(480.f + 136.f * spdFrac), yFromTop(464.f)), ps(9.f), rgba4F(pal.accentSoft, 1.f));

        auto spdStepBtnSpr = ui::makeCustomMenuBtn(fmt::format("{}%", spdPct).c_str(), ps(68.f), ps(34.f), pal.elevated, pal.line, 0.21f, false, ps(8.f));
        auto spdStepBtn = CCMenuItemExt::createSpriteExtra(spdStepBtnSpr, [this, spdPct](CCMenuItemSpriteExtra*) {
            int nextSpd = (spdPct >= 200) ? 50 : (spdPct + 25);
            ThemeConfig::setRgbSpeedPct(nextSpd);
            this->scheduleRebuild();
        });
        spdStepBtn->m_scaleMultiplier = 1.03f;
        spdStepBtn->setPosition({ps(666.f), yFromTop(464.f)});
        sMenu->addChild(spdStepBtn);

        bool rgbEn = ThemeConfig::isRgbEnabled();
        auto swNode = CCNode::create();
        swNode->setContentSize({ps(64.f), ps(34.f)});
        auto swDn = CCDrawNode::create();
        ui::drawRoundedRect(
            swDn, 0.f, 0.f, ps(64.f), ps(34.f), ps(17.f),
            rgba4F(rgbEn ? pal.accent : pal.bgOuter, 1.f),
            ps(1.2f), rgba4F(rgbEn ? pal.accentSoft : pal.line, 0.9f)
        );
        swDn->drawDot(
            ccp(rgbEn ? ps(47.f) : ps(17.f), ps(17.f)),
            ps(12.f),
            rgba4F(rgbEn ? pal.inkOnAccent : pal.text3, 1.f)
        );
        swNode->addChild(swDn);
        auto swBtn = CCMenuItemExt::createSpriteExtra(swNode, [this, rgbEn](CCMenuItemSpriteExtra*) {
            ThemeConfig::setRgbEnabled(!rgbEn);
            this->scheduleRebuild();
        });
        swBtn->m_scaleMultiplier = 1.03f;
        swBtn->setPosition({ps(752.f), yFromTop(464.f)});
        sMenu->addChild(swBtn);

        // --- TARJETA 5: Color de bordes, pinchos y robot RGB (yTop=526, h=104) ---
        drawSettingCard(526.f, 104.f);
        if (auto t5 = ui::createStyledLabel("Color de bordes, pinchos y robot", true, 0.24f, pal.text, ps(420.f))) {
            t5->setAnchorPoint({0.f, 0.5f});
            t5->setPosition({ps(24.f), yFromTop(558.f)});
            content->addChild(t5, 5);
        }
        if (auto d5 = ui::createStyledLabel("Modifica el color de pinchos y robot con la rueda oficial.", false, 0.39f, pal.text3, ps(430.f))) {
            d5->setAnchorPoint({0.f, 0.5f});
            d5->setPosition({ps(24.f), yFromTop(592.f)});
            content->addChild(d5, 5);
        }

        ccColor3B spkCol = ThemeConfig::getSpikesRgbColor();
        ui::drawRoundedRect(
            sDn, ps(464.f), yFromTop(596.f), ps(34.f), ps(34.f), ps(8.f),
            rgba4F(spkCol, 1.f), ps(1.1f), rgba4F(COL_TEXT_WHITE, 0.9f)
        );
        auto spkPickSpr = ui::makeCustomMenuBtn("PINCHOS RGB", ps(118.f), ps(38.f), pal.elevated, pal.lineStrong, 0.20f, false, ps(9.f));
        auto spkPickBtn = CCMenuItemExt::createSpriteExtra(spkPickSpr, [this, spkCol](CCMenuItemSpriteExtra*) {
            m_colorEditTarget = 1;
            this->setEmbeddedPickerColor(spkCol, "RUEDA: PINCHOS RGB");
        });
        spkPickBtn->m_scaleMultiplier = 1.03f;
        spkPickBtn->setPosition({ps(566.f), yFromTop(579.f)});
        sMenu->addChild(spkPickBtn);

        ccColor3B robCol = ThemeConfig::getRobotRgbColor();
        ui::drawRoundedRect(
            sDn, ps(636.f), yFromTop(596.f), ps(34.f), ps(34.f), ps(8.f),
            rgba4F(robCol, 1.f), ps(1.1f), rgba4F(COL_TEXT_WHITE, 0.9f)
        );
        auto robPickSpr = ui::makeCustomMenuBtn("ROBOT RGB", ps(112.f), ps(38.f), pal.elevated, pal.lineStrong, 0.20f, false, ps(9.f));
        auto robPickBtn = CCMenuItemExt::createSpriteExtra(robPickSpr, [this, robCol](CCMenuItemSpriteExtra*) {
            m_colorEditTarget = 2;
            this->setEmbeddedPickerColor(robCol, "RUEDA: ROBOT RGB");
        });
        robPickBtn->m_scaleMultiplier = 1.03f;
        robPickBtn->setPosition({ps(734.f), yFromTop(579.f)});
        sMenu->addChild(robPickBtn);

        // --- TARJETA 6: Boton del editor (yTop=644, h=104) ---
        drawSettingCard(644.f, 104.f);
        if (auto t6 = ui::createStyledLabel("Boton del editor", true, 0.25f, pal.text, ps(320.f))) {
            t6->setAnchorPoint({0.f, 0.5f});
            t6->setPosition({ps(24.f), yFromTop(676.f)});
            content->addChild(t6, 5);
        }
        if (auto d6 = ui::createStyledLabel("Toca para abrir; manten y arrastra para moverlo.", false, 0.39f, pal.text3, ps(340.f))) {
            d6->setAnchorPoint({0.f, 0.5f});
            d6->setPosition({ps(24.f), yFromTop(710.f)});
            content->addChild(d6, 5);
        }

        int opPct = ThemeConfig::getEditorBtnOpacityPct();
        float opFrac = std::clamp((opPct - 20.f) / 80.f, 0.f, 1.f);
        if (auto opLbl = ui::createStyledLabel("Opacidad", false, 0.37f, pal.text3, ps(80.f))) {
            opLbl->setAnchorPoint({1.f, 0.5f});
            opLbl->setPosition({ps(522.f), yFromTop(670.f)});
            content->addChild(opLbl, 5);
        }
        ui::drawRoundedRect(
            sDn, ps(426.f), yFromTop(701.f), ps(96.f), ps(10.f), ps(5.f),
            rgba4F(pal.bgOuter, 1.f), 0.f, ccc4f(0, 0, 0, 0)
        );
        ui::drawRoundedRect(
            sDn, ps(426.f), yFromTop(701.f), ps(std::max(10.f, 96.f * opFrac)), ps(10.f), ps(5.f),
            rgba4F(pal.accent, 1.f), 0.f, ccc4f(0, 0, 0, 0)
        );
        sDn->drawDot(ccp(ps(426.f + 96.f * opFrac), yFromTop(696.f)), ps(8.5f), rgba4F(pal.accentSoft, 1.f));

        auto opStepSpr = ui::makeCustomMenuBtn(fmt::format("{}%", opPct).c_str(), ps(60.f), ps(34.f), pal.elevated, pal.line, 0.20f, false, ps(8.f));
        auto opStepBtn = CCMenuItemExt::createSpriteExtra(opStepSpr, [this, opPct](CCMenuItemSpriteExtra*) {
            int nextOp = (opPct >= 90) ? 35 : (opPct + 15);
            ThemeConfig::setEditorBtnOpacityPct(nextOp);
            this->scheduleRebuild();
        });
        opStepBtn->m_scaleMultiplier = 1.03f;
        opStepBtn->setPosition({ps(564.f), yFromTop(696.f)});
        sMenu->addChild(opStepBtn);

        auto resetPosSpr = ui::makeCustomMenuBtn("RESTABLECER POSICION", ps(184.f), ps(38.f), pal.elevated, pal.lineStrong, 0.19f, false, ps(10.f));
        auto resetPosBtn = CCMenuItemExt::createSpriteExtra(resetPosSpr, [this](CCMenuItemSpriteExtra*) {
            if (auto uiLayer = EditorUI::get()) {
                if (auto btn = typeinfo_cast<DragButton*>(uiLayer->getChildByID("decomind-menu"_spr))) {
                    btn->resetToDefaultPosition();
                }
            }
            if (m_settingsStatusLbl) {
                fitBMFontLabel(m_settingsStatusLbl, "Posicion del boton flotante restablecida.", ps(760.f), 0.42f);
            }
        });
        resetPosBtn->m_scaleMultiplier = 1.03f;
        resetPosBtn->setPosition({ps(698.f), yFromTop(696.f)});
        sMenu->addChild(resetPosBtn);

        // --- TARJETA 7 (NUEVA): Fuente de letras ("cambia la fuente de letras de la ui") (yTop=762, h=104) ---
        drawSettingCard(762.f, 104.f);
        if (auto t7 = ui::createStyledLabel("Fuente de letras", true, 0.25f, pal.text, ps(380.f))) {
            t7->setAnchorPoint({0.f, 0.5f});
            t7->setPosition({ps(24.f), yFromTop(794.f)});
            content->addChild(t7, 5);
        }
        if (auto d7 = ui::createStyledLabel("cambia la fuente de letras de la ui", false, 0.39f, pal.text3, ps(420.f))) {
            d7->setAnchorPoint({0.f, 0.5f});
            d7->setPosition({ps(24.f), yFromTop(828.f)});
            content->addChild(d7, 5);
        }

        // Caja delgada a la derecha con la fuente seleccionada + flechita en el borde derecho
        const float fontBoxW = ps(254.f);
        const float fontBoxH = ps(44.f);
        auto fontBoxNode = CCNode::create();
        fontBoxNode->setContentSize({fontBoxW, fontBoxH});
        auto fbDn = CCDrawNode::create();
        ui::drawRoundedRect(
            fbDn, 0.f, 0.f, fontBoxW, fontBoxH, ps(10.f),
            rgba4F(pal.bgOuter, 0.99f), ps(1.3f), rgba4F(pal.lineStrong, 0.95f)
        );
        // Separador vertical antes de la flechita del borde
        fbDn->drawSegment(
            ccp(fontBoxW - ps(38.f), ps(8.f)),
            ccp(fontBoxW - ps(38.f), fontBoxH - ps(8.f)),
            ps(0.9f), rgba4F(pal.line, 0.8f)
        );
        // Flechita apuntando hacia arriba (indica despliegue hacia arriba)
        CCPoint arrowPts[3] = {
            ccp(fontBoxW - ps(25.f), fontBoxH * 0.40f),
            ccp(fontBoxW - ps(19.f), fontBoxH * 0.62f),
            ccp(fontBoxW - ps(13.f), fontBoxH * 0.40f)
        };
        fbDn->drawPolygon(arrowPts, 3, rgba4F(pal.accentSoft, 1.f), 0.f, ccc4f(0, 0, 0, 0));
        fontBoxNode->addChild(fbDn, 1);

        auto const& curFont = ThemeConfig::getActiveFontEntry();
        if (auto fNameLbl = ui::createStyledLabel(curFont.displayName, false, 0.41f, pal.text, fontBoxW - ps(54.f))) {
            fNameLbl->setAnchorPoint({0.f, 0.5f});
            fNameLbl->setPosition({ps(14.f), fontBoxH * 0.5f});
            fontBoxNode->addChild(fNameLbl, 2);
        }

        auto fontDropBtn = CCMenuItemExt::createSpriteExtra(fontBoxNode, [this](CCMenuItemSpriteExtra*) {
            this->toggleFontDropdownUpward();
        });
        fontDropBtn->m_scaleMultiplier = 1.02f;
        fontDropBtn->setPosition({ps(663.f), yFromTop(814.f)});
        sMenu->addChild(fontDropBtn);

        m_settingsStatusLbl = ui::createStyledLabel(
            "Consejo: Desliza hacia arriba o abajo para ver todas las opciones de Ajustes.",
            false, 0.39f, pal.accentSoft, ps(760.f)
        );
        if (m_settingsStatusLbl) {
            m_settingsStatusLbl->setAnchorPoint({0.f, 0.5f});
            m_settingsStatusLbl->setPosition({ps(24.f), yFromTop(904.f)});
            content->addChild(m_settingsStatusLbl, 5);
        }

        if (m_savedSettingsScrollY <= 0.f && m_savedSettingsScrollY >= m_settingsMinScrollY) {
            content->setPositionY(m_savedSettingsScrollY);
        } else {
            m_settingsScroll->scrollToTop();
        }

        // --- PANEL DERECHO INTEGRADO EN AJUSTES: SELECTOR DE COLOR (x_ref = 1084..1508, y_top_ref = 128..880) ---
        this->buildEmbeddedColorPickerPanel(page);

        // --- MENU DESPLEGABLE HACIA ARRIBA PARA "FUENTE DE LETRAS" ---
        // Mismo ancho exacto (fontBoxW = ps(254.f)) que la caja donde esta escrita la fuente
        this->buildUpwardFontDropdown(page, fontBoxW);
    }

    // Construye el panel derecho "SELECTOR DE COLOR / Rueda oficial de GD" de la Pagina 5 del PDF
    void buildEmbeddedColorPickerPanel(CCNode* page) {
        auto pal = ThemeConfig::getPalette();

        ui::addRefRoundedBox(
            page, 1084.f, 128.f, 424.f, 752.f, 18.f,
            pal.panel, 0.98f, pal.line, 0.90f, 1.4f, 3
        );

        char const* targetName =
            (m_colorEditTarget == 1) ? "RUEDA: PINCHOS RGB" :
            (m_colorEditTarget == 2) ? "RUEDA: ROBOT RGB" : "SELECTOR DE COLOR";
        m_embTargetTitleLbl = ui::createStyledLabel(targetName, true, 0.22f, pal.text2, ps(240.f));
        if (m_embTargetTitleLbl) {
            m_embTargetTitleLbl->setAnchorPoint({0.5f, 0.5f});
            m_embTargetTitleLbl->setPosition({px(1296.f), py(158.f)});
            page->addChild(m_embTargetTitleLbl, 5);
        }

        // Rueda nativa oficial CCControlColourPicker en el centro del panel derecho
        m_embeddedWheel = cocos2d::extension::CCControlColourPicker::colourPicker();
        if (m_embeddedWheel) {
            m_embeddedWheel->setAnchorPoint({0.f, 0.f});
            m_embeddedWheel->setPosition({px(1296.f), py(362.f)});
            m_embeddedWheel->setScale(0.66f);
            m_embeddedWheel->setColorValue(m_embCurColor);
            m_embeddedWheel->setDelegate(this);
            page->addChild(m_embeddedWheel, 10);
        }

        m_embeddedPreviewDn = CCDrawNode::create();
        page->addChild(m_embeddedPreviewDn, 5);

        // Casillas R, G, B (y_top_ref = 548..624)
        char const* rgbNames[3] = {"R", "G", "B"};
        const float rgbBoxX[3] = {1200.f, 1298.f, 1396.f};
        for (int i = 0; i < 3; ++i) {
            ui::addRefRoundedBox(
                page, rgbBoxX[i], 548.f, 88.f, 74.f, 12.f,
                pal.bgOuter, 0.98f, pal.line, 0.90f, 1.1f, 4
            );
            if (auto rl = ui::createStyledLabel(rgbNames[i], false, 0.36f, pal.text3, ps(40.f))) {
                rl->setAnchorPoint({0.f, 0.5f});
                rl->setPosition({px(rgbBoxX[i] + 14.f), py(568.f)});
                page->addChild(rl, 5);
            }
            auto inp = TextInput::create(ps(74.f) / 0.72f, "155", "bigFont.fnt");
            inp->setFilter("0123456789");
            inp->setMaxCharCount(3);
            inp->setScale(0.72f);
            if (inp->getBGSprite()) inp->getBGSprite()->setOpacity(0);
            inp->setPosition({px(rgbBoxX[i] + 44.f), py(598.f)});
            inp->setCallback([this, i](std::string const& str) {
                if (m_embUpdatingInputs || str.empty()) return;
                int v = std::clamp(static_cast<int>(std::strtol(str.c_str(), nullptr, 10)), 0, 255);
                if (i == 0) m_embCurColor.r = static_cast<GLubyte>(v);
                else if (i == 1) m_embCurColor.g = static_cast<GLubyte>(v);
                else m_embCurColor.b = static_cast<GLubyte>(v);
                m_embUpdatingInputs = true;
                if (m_embeddedWheel) m_embeddedWheel->setColorValue(m_embCurColor);
                this->updateEmbeddedColorVisuals(m_embCurColor);
                if (m_embHexInput) {
                    m_embHexInput->setString(fmt::format(
                        "{:02X}{:02X}{:02X}",
                        static_cast<int>(m_embCurColor.r),
                        static_cast<int>(m_embCurColor.g),
                        static_cast<int>(m_embCurColor.b)
                    ));
                }
                m_embUpdatingInputs = false;
            });
            page->addChild(inp, 12);
            if (i == 0) m_embRInput = inp;
            else if (i == 1) m_embGInput = inp;
            else m_embBInput = inp;
        }

        // Caja HEX + botones COPY, PASTE, DEFAULT (y_top_ref = 642..690)
        ui::addRefRoundedBox(
            page, 1108.f, 642.f, 146.f, 48.f, 12.f,
            pal.bgOuter, 0.98f, pal.line, 0.90f, 1.1f, 4
        );
        if (auto hxLbl = ui::createStyledLabel("HEX #", false, 0.36f, pal.text3, ps(54.f))) {
            hxLbl->setAnchorPoint({0.f, 0.5f});
            hxLbl->setPosition({px(1120.f), py(666.f)});
            page->addChild(hxLbl, 5);
        }
        m_embHexInput = TextInput::create(ps(84.f) / 0.68f, "9B5CF6", "bigFont.fnt");
        m_embHexInput->setFilter("0123456789abcdefABCDEF");
        m_embHexInput->setMaxCharCount(6);
        m_embHexInput->setScale(0.68f);
        if (m_embHexInput->getBGSprite()) m_embHexInput->getBGSprite()->setOpacity(0);
        m_embHexInput->setPosition({px(1206.f), py(666.f)});
        m_embHexInput->setCallback([this](std::string const& str) {
            if (m_embUpdatingInputs || str.size() != 6) return;
            unsigned int v = std::strtoul(str.c_str(), nullptr, 16);
            m_embCurColor = ccc3(
                static_cast<GLubyte>((v >> 16) & 0xFF),
                static_cast<GLubyte>((v >> 8) & 0xFF),
                static_cast<GLubyte>(v & 0xFF)
            );
            m_embUpdatingInputs = true;
            if (m_embeddedWheel) m_embeddedWheel->setColorValue(m_embCurColor);
            this->updateEmbeddedColorVisuals(m_embCurColor);
            this->syncEmbeddedRgbInputs(m_embCurColor);
            m_embUpdatingInputs = false;
        });
        page->addChild(m_embHexInput, 12);

        auto pMenu = CCMenu::create();
        pMenu->setPosition({0.f, 0.f});
        page->addChild(pMenu, 15);

        auto cpSpr = ui::makeCustomMenuBtn("COPY", ps(68.f), ps(48.f), pal.card, pal.line, 0.20f, false, ps(10.f));
        auto cpBtn = CCMenuItemExt::createSpriteExtra(cpSpr, [this](CCMenuItemSpriteExtra*) {
            ThemeConfig::setClipboardColor(m_embCurColor);
        });
        cpBtn->m_scaleMultiplier = 1.03f;
        cpBtn->setPosition({px(1296.f), py(666.f)});
        pMenu->addChild(cpBtn);

        auto psSpr = ui::makeCustomMenuBtn("PASTE", ps(68.f), ps(48.f), pal.card, pal.line, 0.20f, false, ps(10.f));
        auto psBtn = CCMenuItemExt::createSpriteExtra(psSpr, [this](CCMenuItemSpriteExtra*) {
            ccColor3B clip = ThemeConfig::getClipboardColor();
            this->setEmbeddedPickerColor(clip, nullptr);
        });
        psBtn->m_scaleMultiplier = 1.03f;
        psBtn->setPosition({px(1370.f), py(666.f)});
        pMenu->addChild(psBtn);

        auto dfSpr = ui::makeCustomMenuBtn("DEFAULT", ps(76.f), ps(48.f), pal.card, pal.line, 0.19f, false, ps(10.f));
        auto dfBtn = CCMenuItemExt::createSpriteExtra(dfSpr, [this](CCMenuItemSpriteExtra*) {
            ccColor3B defCol = ccc3(155, 92, 246);
            if (m_colorEditTarget == 1) ThemeConfig::resetSpikesRgbToTheme();
            if (m_colorEditTarget == 2) ThemeConfig::resetRobotRgbToTheme();
            this->setEmbeddedPickerColor(defCol, nullptr);
        });
        dfBtn->m_scaleMultiplier = 1.03f;
        dfBtn->setPosition({px(1448.f), py(666.f)});
        pMenu->addChild(dfBtn);

        // Barra inferior grande "OK" en color Acento con texto oscuro (fiel a Pagina 5 del PDF)
        auto okSpr = ui::makeCustomMenuBtn("OK", ps(376.f), ps(58.f), pal.accent, pal.accent, 0.28f, true, ps(14.f));
        auto okBtn = CCMenuItemExt::createSpriteExtra(okSpr, [this](CCMenuItemSpriteExtra*) {
            if (m_colorEditTarget == 1) {
                ThemeConfig::setSpikesRgbColor(m_embCurColor);
            } else if (m_colorEditTarget == 2) {
                ThemeConfig::setRobotRgbColor(m_embCurColor);
            } else {
                ThemeConfig::setMenuAccent(m_embCurColor);
            }
            this->scheduleRebuild();
        });
        okBtn->m_scaleMultiplier = 1.02f;
        okBtn->setPosition({px(1296.f), py(822.f)});
        pMenu->addChild(okBtn);

        m_embUpdatingInputs = true;
        this->updateEmbeddedColorVisuals(m_embCurColor);
        this->syncEmbeddedRgbInputs(m_embCurColor);
        if (m_embHexInput) {
            m_embHexInput->setString(fmt::format(
                "{:02X}{:02X}{:02X}",
                static_cast<int>(m_embCurColor.r),
                static_cast<int>(m_embCurColor.g),
                static_cast<int>(m_embCurColor.b)
            ));
        }
        m_embUpdatingInputs = false;
    }

    void setEmbeddedPickerColor(ccColor3B c, char const* newTitle) {
        m_embCurColor = c;
        if (newTitle && m_embTargetTitleLbl) {
            fitBMFontLabel(m_embTargetTitleLbl, newTitle, ps(240.f), 0.22f, true);
        }
        m_embUpdatingInputs = true;
        if (m_embeddedWheel) m_embeddedWheel->setColorValue(c);
        this->updateEmbeddedColorVisuals(c);
        this->syncEmbeddedRgbInputs(c);
        if (m_embHexInput) {
            m_embHexInput->setString(fmt::format(
                "{:02X}{:02X}{:02X}",
                static_cast<int>(c.r),
                static_cast<int>(c.g),
                static_cast<int>(c.b)
            ));
        }
        m_embUpdatingInputs = false;
    }

    void updateEmbeddedColorVisuals(ccColor3B c) {
        if (!m_embeddedPreviewDn) return;
        m_embeddedPreviewDn->clear();
        auto pal = ThemeConfig::getPalette();
        // Muestra grande a la izquierda de R, G, B (como en Pagina 5 del PDF)
        ui::drawRoundedRect(
            m_embeddedPreviewDn,
            px(1108.f), py(622.f), ps(76.f), ps(74.f), ps(14.f),
            rgba4F(c, 1.f), ps(1.4f), rgba4F(COL_TEXT_WHITE, 0.92f)
        );
        ui::drawRoundedRect(
            m_embeddedPreviewDn,
            px(1160.f), py(622.f), ps(24.f), ps(24.f), ps(6.f),
            rgba4F(pal.accentDark, 1.f), ps(1.0f), rgba4F(pal.line, 0.9f)
        );
    }

    void syncEmbeddedRgbInputs(ccColor3B c) {
        if (m_embRInput) m_embRInput->setString(fmt::format("{}", static_cast<int>(c.r)));
        if (m_embGInput) m_embGInput->setString(fmt::format("{}", static_cast<int>(c.g)));
        if (m_embBInput) m_embBInput->setString(fmt::format("{}", static_cast<int>(c.b)));
    }

    void colorValueChanged(ccColor3B c) override {
        m_embCurColor = c;
        this->updateEmbeddedColorVisuals(c);
        if (!m_embUpdatingInputs) {
            m_embUpdatingInputs = true;
            this->syncEmbeddedRgbInputs(c);
            if (m_embHexInput) {
                m_embHexInput->setString(fmt::format(
                    "{:02X}{:02X}{:02X}",
                    static_cast<int>(c.r),
                    static_cast<int>(c.g),
                    static_cast<int>(c.b)
                ));
            }
            m_embUpdatingInputs = false;
        }
    }

    // -----------------------------------------------------------------------
    // 8. MENU DESPLEGABLE HACIA ARRIBA PARA "FUENTE DE LETRAS" Y OVERLAY DE CARGA
    // -----------------------------------------------------------------------
    void buildUpwardFontDropdown(CCNode* page, float dropW) {
        auto pal = ThemeConfig::getPalette();
        float dropH = ps(260.f);

        m_fontDropdownOverlay = CCNode::create();
        m_fontDropdownOverlay->setContentSize({dropW, dropH});
        m_fontDropdownOverlay->ignoreAnchorPointForPosition(false);
        m_fontDropdownOverlay->setAnchorPoint({0.5f, 0.f});
        // Se despliega exactamente hacia arriba sobre la caja de seleccion de fuente (x_ref=917)
        m_fontDropdownOverlay->setPosition({px(917.f), py(760.f)});
        m_fontDropdownOverlay->setVisible(false);
        page->addChild(m_fontDropdownOverlay, 80);

        auto bgDn = CCDrawNode::create();
        // Menu deslizante transparente color oscuro delgado del mismo ancho que la caja
        ui::drawRoundedRect(
            bgDn, 0.f, 0.f, dropW, dropH, ps(10.f),
            rgba4F(ccc3(0x07, 0x05, 0x0e), 0.91f),
            ps(1.3f),
            rgba4F(pal.accent, 0.88f)
        );
        m_fontDropdownOverlay->addChild(bgDn, 1);

        m_fontDropdownScroll = ScrollLayer::create(CCSize{dropW - ps(6.f), dropH - ps(8.f)});
        m_fontDropdownScroll->ignoreAnchorPointForPosition(false);
        m_fontDropdownScroll->setAnchorPoint({0.5f, 0.5f});
        m_fontDropdownScroll->setPosition({dropW * 0.5f, dropH * 0.5f});
        m_fontDropdownOverlay->addChild(m_fontDropdownScroll, 5);

        auto content = m_fontDropdownScroll->m_contentLayer;
        const float itemH = ps(36.f);
        float totalH = std::max(dropH - ps(8.f), GAME_FONT_COUNT * itemH + ps(8.f));
        content->setContentSize({dropW - ps(6.f), totalH});
        m_fontDropMinScrollY = (dropH - ps(8.f)) - totalH;

        int activeIdx = ThemeConfig::getFontIndex();
        for (int i = 0; i < GAME_FONT_COUNT; ++i) {
            float iy = totalH - ps(4.f) - (i + 1) * itemH;
            bool isCur = (i == activeIdx);

            auto row = CCNode::create();
            row->setContentSize({dropW - ps(6.f), itemH});
            row->setPosition({0.f, iy});

            auto rDn = CCDrawNode::create();
            if (isCur) {
                ui::drawRoundedRect(
                    rDn, ps(4.f), ps(2.f), dropW - ps(14.f), itemH - ps(4.f), ps(6.f),
                    rgba4F(pal.accent, 0.92f), 0.f, ccc4f(0, 0, 0, 0)
                );
            } else {
                rDn->drawSegment(
                    ccp(ps(8.f), 0.f), ccp(dropW - ps(14.f), 0.f),
                    ps(0.6f), rgba4F(pal.line, 0.45f)
                );
            }
            row->addChild(rDn, 1);

            std::string disp = GAME_FONT_LIST[i].displayName;
            if (GAME_FONT_LIST[i].uppercaseOnly) {
                for (char& ch : disp) {
                    if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
                }
            }
            auto fLbl = CCLabelBMFont::create(disp.c_str(), GAME_FONT_LIST[i].fntFile);
            if (!fLbl) {
                fLbl = CCLabelBMFont::create(GAME_FONT_LIST[i].displayName, "chatFont.fnt");
            }
            if (fLbl) {
                float sc = 0.40f * GAME_FONT_LIST[i].scaleMul;
                fLbl->setAnchorPoint({0.f, 0.5f});
                fLbl->setPosition({ps(14.f), itemH * 0.5f});
                fLbl->setColor(isCur ? pal.inkOnAccent : pal.text);
                fLbl->limitLabelWidth(dropW - ps(32.f), sc, 0.12f);
                row->addChild(fLbl, 2);
            }

            content->addChild(row, 3);
        }
        m_fontDropdownScroll->scrollToTop();
    }

    void toggleFontDropdownUpward() {
        if (!m_fontDropdownOverlay) return;
        m_fontDropdownOpen = !m_fontDropdownOpen;
        m_fontDropdownOverlay->setVisible(m_fontDropdownOpen);
        if (m_settingsScroll) m_settingsScroll->setTouchEnabled(!m_fontDropdownOpen);
    }

    // Activa la pantalla ligeramente oscurecida y borrosa con el mensaje
    // "Cargando fuente..." y el circulo giratorio en el centro de la pantalla
    void startFontLoadingTransition(int newFontIdx) {
        if (m_fontDropdownOverlay) {
            m_fontDropdownOverlay->setVisible(false);
            m_fontDropdownOpen = false;
            if (m_settingsScroll) m_settingsScroll->setTouchEnabled(true);
        }
        if (m_fontLoadingOverlay) {
            m_fontLoadingOverlay->removeFromParent();
            m_fontLoadingOverlay = nullptr;
        }

        auto pal = ThemeConfig::getPalette();
        m_pendingFontIndex = std::clamp(newFontIdx, 0, GAME_FONT_COUNT - 1);
        m_fontLoadingRemaining = 0.65f;

        m_fontLoadingOverlay = CCNode::create();
        m_fontLoadingOverlay->setContentSize({PW, PH});
        m_fontLoadingOverlay->setPosition({0.f, 0.f});

        // 2. Circulo giratorio tipico de carga en el centro
        auto spinnerHolder = CCNode::create();
        spinnerHolder->setPosition({PW * 0.5f, PH * 0.5f + ps(32.f)});
        m_fontLoadingOverlay->addChild(spinnerHolder, 5);

        if (auto nativeSpr = CCSprite::create("loadingCircle.png")) {
            nativeSpr->setScale(0.65f);
            nativeSpr->setColor(pal.accentSoft);
            spinnerHolder->addChild(nativeSpr, 2);
        } else {
            auto spinDn = CCDrawNode::create();
            const int arcSegs = 28;
            const float radius = ps(26.f);
            for (int s = 0; s < arcSegs; ++s) {
                float a1 = (6.2831853f * 0.78f) * (static_cast<float>(s) / static_cast<float>(arcSegs));
                float a2 = (6.2831853f * 0.78f) * (static_cast<float>(s + 1) / static_cast<float>(arcSegs));
                float prog = static_cast<float>(s + 1) / static_cast<float>(arcSegs);
                ccColor3B c = lerpColor(pal.accentDark, pal.accentSoft, prog);
                spinDn->drawSegment(
                    ccp(std::cos(a1) * radius, std::sin(a1) * radius),
                    ccp(std::cos(a2) * radius, std::sin(a2) * radius),
                    ps(3.2f),
                    rgba4F(c, 0.18f + 0.82f * prog)
                );
            }
            spinnerHolder->addChild(spinDn, 2);
        }
        spinnerHolder->runAction(CCRepeatForever::create(CCRotateBy::create(0.75f, 360.f)));

        // 3. Mensaje "Cargando fuente..." y nombre de la fuente elegida
        auto loadLbl = CCLabelBMFont::create("Cargando fuente...", "bigFont.fnt");
        if (loadLbl) {
            loadLbl->setColor(pal.text);
            loadLbl->setScale(0.32f);
            loadLbl->setPosition({PW * 0.5f, PH * 0.5f - ps(12.f)});
            m_fontLoadingOverlay->addChild(loadLbl, 6);
        }
        auto subLbl = CCLabelBMFont::create(GAME_FONT_LIST[m_pendingFontIndex].displayName, "chatFont.fnt");
        if (subLbl) {
            subLbl->setColor(pal.accentSoft);
            subLbl->setScale(0.46f);
            subLbl->setPosition({PW * 0.5f, PH * 0.5f - ps(40.f)});
            m_fontLoadingOverlay->addChild(subLbl, 6);
        }

        m_mainLayer->addChild(m_fontLoadingOverlay, 500);
    }

    // -----------------------------------------------------------------------
    // 9. DESLIZAMIENTO TACTIL CON EL DEDO (INICIO, INFORMACION, AJUSTES Y DROPDOWN)
    // -----------------------------------------------------------------------
    bool hasChildModalOpen() {
        if (auto scene = CCDirector::get()->getRunningScene()) {
            if (auto children = scene->getChildren()) {
                for (unsigned int i = 0; i < children->count(); ++i) {
                    auto* obj = children->objectAtIndex(i);
                    if (obj != this && typeinfo_cast<FLAlertLayer*>(obj)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    static bool isTouchInsideScroll(CCTouch* t, ScrollLayer* s) {
        if (!t || !s || !s->isVisible()) return false;
        CCPoint local = s->convertToNodeSpace(t->getLocation());
        CCSize sz = s->getContentSize();
        return (local.x >= 0.f && local.x <= sz.width && local.y >= 0.f && local.y <= sz.height);
    }

    int hitTestNodeArray(CCTouch* t, CCNode** nodes, int count) {
        if (!t) return -1;
        for (int i = 0; i < count; ++i) {
            if (!nodes[i]) continue;
            CCPoint local = nodes[i]->convertToNodeSpace(t->getLocation());
            CCSize sz = nodes[i]->getContentSize();
            if (local.x >= 0.f && local.x <= sz.width && local.y >= 0.f && local.y <= sz.height) {
                return i;
            }
        }
        return -1;
    }

    bool ccTouchBegan(CCTouch* t, CCEvent* e) override {
        if (!t || this->hasChildModalOpen() || m_fontLoadingOverlay != nullptr) {
            return true;
        }

        // 1. Si el menu desplegable de fuentes esta abierto en Ajustes
        if (m_activePage == 4 && m_fontDropdownOpen && m_fontDropdownOverlay && m_fontDropdownScroll) {
            if (isTouchInsideScroll(t, m_fontDropdownScroll) && m_fontDropdownScroll->m_contentLayer) {
                m_touchScrollTarget = 4;
                m_touchDragging = false;
                m_touchStartPt = t->getLocation();
                m_touchStartContentY = m_fontDropdownScroll->m_contentLayer->getPositionY();
                CCPoint cl = m_fontDropdownScroll->m_contentLayer->convertToNodeSpace(t->getLocation());
                float totalH = m_fontDropdownScroll->m_contentLayer->getContentSize().height;
                float itemH = ps(36.f);
                int idx = static_cast<int>((totalH - ps(4.f) - cl.y) / itemH);
                m_pressedItemIdx = (idx >= 0 && idx < GAME_FONT_COUNT) ? idx : -1;
                return true;
            } else {
                // Tocar fuera del dropdown lo cierra
                m_fontDropdownOpen = false;
                m_fontDropdownOverlay->setVisible(false);
                if (m_settingsScroll) m_settingsScroll->setTouchEnabled(true);
                return true;
            }
        }

        // 2. Deslizamiento con el dedo y seleccion de tarjetas en INICIO (Pagina 0)
        if (m_activePage == 0 && isTouchInsideScroll(t, m_inicioScroll) && m_inicioScroll->m_contentLayer) {
            m_touchScrollTarget = 1;
            m_touchDragging = false;
            m_touchStartPt = t->getLocation();
            m_touchStartContentY = m_inicioScroll->m_contentLayer->getPositionY();
            m_pressedItemIdx = this->hitTestNodeArray(t, m_cardNodes, 9);
            if (m_pressedItemIdx >= 0 && m_cardNodes[m_pressedItemIdx]) {
                m_cardNodes[m_pressedItemIdx]->setScale(1.018f);
            }
            return true;
        }

        // 3. Deslizamiento con el dedo y seleccion en la lista de modos de INFORMACION (Pagina 1)
        if (m_activePage == 1 && isTouchInsideScroll(t, m_infoListScroll) && m_infoListScroll->m_contentLayer) {
            m_touchScrollTarget = 2;
            m_touchDragging = false;
            m_touchStartPt = t->getLocation();
            m_touchStartContentY = m_infoListScroll->m_contentLayer->getPositionY();
            m_pressedItemIdx = this->hitTestNodeArray(t, m_infoRowNodes, 9);
            return true;
        }

        // 4. Deslizamiento con el dedo en la columna de AJUSTES (Pagina 4)
        // Primero damos prioridad a los botones/controles de sMenu; si el usuario arrastra, desplazamos
        if (m_activePage == 4 && isTouchInsideScroll(t, m_settingsScroll) && m_settingsScroll->m_contentLayer) {
            m_touchScrollTarget = 3;
            m_touchDragging = false;
            m_touchStartPt = t->getLocation();
            m_touchStartContentY = m_settingsScroll->m_contentLayer->getPositionY();
        }

        return Popup::ccTouchBegan(t, e);
    }

    void ccTouchMoved(CCTouch* t, CCEvent* e) override {
        if (!t || m_touchScrollTarget == 0) {
            Popup::ccTouchMoved(t, e);
            return;
        }

        CCPoint delta = t->getLocation() - m_touchStartPt;
        if (!m_touchDragging && ccpLength(delta) > 5.5f) {
            m_touchDragging = true;
            if (m_touchScrollTarget == 1 && m_pressedItemIdx >= 0 && m_cardNodes[m_pressedItemIdx]) {
                m_cardNodes[m_pressedItemIdx]->setScale(1.0f);
            }
        }

        if (m_touchDragging) {
            ScrollLayer* targetScroll = nullptr;
            float minY = 0.f;
            if (m_touchScrollTarget == 1) { targetScroll = m_inicioScroll; minY = m_inicioMinScrollY; }
            else if (m_touchScrollTarget == 2) { targetScroll = m_infoListScroll; minY = m_infoMinScrollY; }
            else if (m_touchScrollTarget == 3) { targetScroll = m_settingsScroll; minY = m_settingsMinScrollY; }
            else if (m_touchScrollTarget == 4) { targetScroll = m_fontDropdownScroll; minY = m_fontDropMinScrollY; }

            if (targetScroll && targetScroll->m_contentLayer) {
                CCPoint curLocal = targetScroll->convertToNodeSpace(t->getLocation());
                CCPoint startLocal = targetScroll->convertToNodeSpace(m_touchStartPt);
                float localDy = curLocal.y - startLocal.y;
                float newY = std::clamp(m_touchStartContentY + localDy, minY, 0.f);
                targetScroll->m_contentLayer->setPositionY(newY);
                if (m_touchScrollTarget == 1) {
                    this->updateInicioScrollbar();
                }
            }
            return;
        }

        Popup::ccTouchMoved(t, e);
    }

    void ccTouchEnded(CCTouch* t, CCEvent* e) override {
        if (m_touchScrollTarget == 1) {
            if (m_pressedItemIdx >= 0 && m_cardNodes[m_pressedItemIdx]) {
                m_cardNodes[m_pressedItemIdx]->setScale(1.0f);
            }
            if (!m_touchDragging && m_pressedItemIdx >= 0 && m_pressedItemIdx < 9) {
                this->selectMode(m_pressedItemIdx);
                this->refreshSelectedModeCards();
            }
            m_touchScrollTarget = 0;
            m_touchDragging = false;
            m_pressedItemIdx = -1;
            return;
        }

        if (m_touchScrollTarget == 2) {
            if (!m_touchDragging && m_pressedItemIdx >= 0 && m_pressedItemIdx < 9) {
                this->selectMode(m_pressedItemIdx);
                this->refreshSelectedModeCards();
            }
            m_touchScrollTarget = 0;
            m_touchDragging = false;
            m_pressedItemIdx = -1;
            return;
        }

        if (m_touchScrollTarget == 4) {
            if (!m_touchDragging && m_pressedItemIdx >= 0 && m_pressedItemIdx < GAME_FONT_COUNT) {
                this->startFontLoadingTransition(m_pressedItemIdx);
            }
            m_touchScrollTarget = 0;
            m_touchDragging = false;
            m_pressedItemIdx = -1;
            return;
        }

        m_touchScrollTarget = 0;
        m_touchDragging = false;
        m_pressedItemIdx = -1;
        Popup::ccTouchEnded(t, e);
    }

    void ccTouchCancelled(CCTouch* t, CCEvent* e) override {
        if (m_pressedItemIdx >= 0 && m_pressedItemIdx < 9 && m_cardNodes[m_pressedItemIdx]) {
            m_cardNodes[m_pressedItemIdx]->setScale(1.0f);
        }
        m_touchScrollTarget = 0;
        m_touchDragging = false;
        m_pressedItemIdx = -1;
        Popup::ccTouchCancelled(t, e);
    }

    void selectMode(int idx) {
        if (idx < 0 || idx >= 9) idx = 0;
        m_selectedMode = idx;

        if (m_activeModeValLbl) {
            fitBMFontLabel(m_activeModeValLbl, MODES[m_selectedMode].name, ps(98.f), 0.40f);
        }
        if (m_chatModeBtnLbl) fitBMFontLabel(m_chatModeBtnLbl, MODES[m_selectedMode].name, ps(180.f), 0.41f);
        this->refreshPage1Detail();
        this->refreshSelectedModeCards();
    }

    void refreshSelectedModeCards() {
        auto pal = ThemeConfig::getPalette();
        for (int i = 0; i < 9; ++i) {
            if (!m_cardNodes[i]) continue;
            m_cardNodes[i]->removeChildByTag(791);
            m_cardNodes[i]->removeChildByTag(792);
            m_cardNodes[i]->removeChildByTag(793);
            if (auto dn = typeinfo_cast<CCDrawNode*>(m_cardNodes[i]->getChildByTag(790))) {
                dn->clear();
                ui::drawRoundedRect(dn, 0.f, 0.f, ps(264.f), ps(214.f), ps(16.f),
                    rgba4F(i == m_selectedMode ? pal.elevated : pal.card, .98f),
                    ps(i == m_selectedMode ? 1.8f : 1.f),
                    rgba4F(i == m_selectedMode ? pal.accent : pal.line, .9f));
            }
            auto badge = ui::createModeIconBadge(i, i == m_selectedMode, 50.f, 13.f);
            badge->setTag(791);
            badge->ignoreAnchorPointForPosition(false);
            badge->setAnchorPoint({0.f, 1.f});
            badge->setPosition({ps(22.f), ps(214.f) - ps(22.f)});
            m_cardNodes[i]->addChild(badge, 4);
            if (i == m_selectedMode) {
                auto check = CCDrawNode::create();
                check->setTag(792);
                auto center = ccp(ps(264.f - 34.f), ps(214.f - 46.f));
                ui::drawCircleShape(check, center, ps(12.f), rgba4F(pal.accent), 0.f, ccc4f(0,0,0,0), 20);
                m_cardNodes[i]->addChild(check, 4);
                if (auto spr = ui::createFitSprite("ui-check.png"_spr, ps(13.f), ps(13.f), center)) {
                    spr->setTag(793);
                    spr->setColor(pal.inkOnAccent);
                    m_cardNodes[i]->addChild(spr, 5);
                }
            }
        }
        for (int i = 0; i < 9; ++i) {
            auto row = m_infoRowNodes[i];
            if (!row) continue;
            bool selected = i == m_selectedMode;
            row->removeChildByTag(791);
            row->removeChildByTag(792);
            if (auto dn = typeinfo_cast<CCDrawNode*>(row->getChildByTag(790))) {
                dn->clear();
                ui::drawRoundedRect(dn, 0.f, 0.f, ps(386.f), ps(68.f), ps(14.f),
                    rgba4F(selected ? pal.elevated : pal.card, .98f),
                    ps(selected ? 1.8f : 1.f), rgba4F(selected ? pal.accent : pal.line, .96f));
            }
            auto badge = ui::createModeIconBadge(i, selected, 42.f, 11.f);
            badge->setTag(791);
            badge->ignoreAnchorPointForPosition(false);
            badge->setAnchorPoint({.5f, .5f});
            badge->setPosition({ps(34.f), ps(34.f)});
            row->addChild(badge, 3);
        }
        this->refreshUsagePanel();
    }

    // -----------------------------------------------------------------------
    // 10. ENTRADA DE TEXTO DEL CHAT Y BARRA INFERIOR
    // -----------------------------------------------------------------------
    void hideInternalPromptInputLabels() {
        if (!m_promptInput) return;
        if (auto* inNode = m_promptInput->getInputNode()) {
            if (auto children = inNode->getChildren()) {
                for (unsigned int i = 0; i < children->count(); ++i) {
                    if (auto* bm = typeinfo_cast<CCLabelBMFont*>(children->objectAtIndex(i))) {
                        bm->setOpacity(0);
                        bm->setScale(0.0001f);
                    }
                }
            }
        }
    }

    void onPromptTextChanged(std::string const& text) {
        this->hideInternalPromptInputLabels();

        std::string norm = normalizePromptInputWithEnye(text);
        size_t firstNonSpace = norm.find_first_not_of(" \t\r\n");
        std::string work = (firstNonSpace == std::string::npos) ? "" : norm.substr(firstNonSpace);

        if (m_placeholderNode) {
            m_placeholderNode->setVisible(work.empty());
        }
        if (m_charCountLbl) {
            if (m_activePage == 2) {
                fitBMFontLabel(m_charCountLbl, fmt::format("{} / 160", work.size()), ps(160.f), 0.37f);
            } else {
                fitBMFontLabel(m_charCountLbl, fmt::format("{} / 160 · prueba /catalogo", work.size()), ps(260.f), 0.37f);
            }
        }

        for (int l = 0; l < 4; ++l) {
            if (m_typedPreviewLines[l]) m_typedPreviewLines[l]->setString("");
        }
        if (m_tildeOverlayNode) {
            m_tildeOverlayNode->removeAllChildren();
        }
        if (work.empty()) return;

        bool wideMode = (m_activePage == 2);
        size_t maxLineChars = wideMode ? 76 : 38;
        float maxW = wideMode ? ps(720.f) : ps(320.f);
        auto pal = ThemeConfig::getPalette();

        std::string rest = work;
        for (int l = 0; l < 4 && !rest.empty(); ++l) {
            size_t lead = rest.find_first_not_of(' ');
            if (lead == std::string::npos) break;
            if (lead > 0) rest = rest.substr(lead);

            std::string chunk;
            if (l < 3 && rest.size() > maxLineChars) {
                size_t splitPos = rest.rfind(' ', maxLineChars);
                if (splitPos != std::string::npos && splitPos > 0) {
                    chunk = rest.substr(0, splitPos);
                    rest = rest.substr(splitPos + 1);
                } else {
                    chunk = rest.substr(0, maxLineChars);
                    rest = rest.substr(maxLineChars);
                }
            } else {
                chunk = rest;
                rest.clear();
            }

            if (!m_typedPreviewLines[l]) continue;

            std::string displayLine = chunk;
            for (char& ch : displayLine) {
                if (ch == '~') ch = 'n';
                else if (ch == '^') ch = 'N';
            }

            m_typedPreviewLines[l]->setString(displayLine.c_str());
            m_typedPreviewLines[l]->setColor(pal.text);
            m_typedPreviewLines[l]->setScale(0.43f);
            float w = m_typedPreviewLines[l]->getContentSize().width * 0.43f;
            if (w > maxW && w > 0.f) {
                m_typedPreviewLines[l]->setScale(std::max(0.18f, 0.43f * (maxW / w)));
            }

            if (m_tildeOverlayNode) {
                float lblScale = m_typedPreviewLines[l]->getScale();
                CCPoint basePos = m_typedPreviewLines[l]->getPosition();
                float halfH = m_typedPreviewLines[l]->getContentSize().height * 0.5f;
                for (size_t k = 0; k < chunk.size(); ++k) {
                    if (chunk[k] != '~' && chunk[k] != '^') continue;
                    if (auto* chSpr = typeinfo_cast<CCSprite*>(m_typedPreviewLines[l]->getChildByTag(static_cast<int>(k)))) {
                        auto tilde = CCLabelBMFont::create("~", "chatFont.fnt");
                        tilde->setColor(pal.text);
                        tilde->setScale(lblScale * 0.80f);
                        CCPoint sprPos = chSpr->getPosition();
                        float tildeOffsetY = (chunk[k] == '^') ? ps(8.2f) : ps(6.4f);
                        tilde->setPosition({
                            basePos.x + sprPos.x * lblScale,
                            basePos.y + (sprPos.y - halfH) * lblScale + tildeOffsetY
                        });
                        m_tildeOverlayNode->addChild(tilde);
                    }
                }
            }
        }
    }

    void setPromptText(std::string const& text) {
        std::string norm = normalizePromptInputWithEnye(text);
        m_savedPromptText = norm;
        if (m_promptInput) {
            m_promptFieldDelegate.m_internalSync = true;
            m_promptInput->setString(norm);
            m_promptFieldDelegate.m_internalSync = false;
        }
        this->onPromptTextChanged(norm);
    }

    void buildBottomBar() {
        auto pal = ThemeConfig::getPalette();

        // Barra inferior izquierda (5 accesos con icono a la izquierda y texto a la derecha, fiel al PDF)
        ui::addRefRoundedBox(
            m_mainLayer,
            28.f, 894.f, 1066.f, 96.f, 18.f,
            pal.panel, 0.98f, pal.line, 0.90f, 1.4f, 3
        );

        const float btnCentersX[5] = {136.f, 344.f, 560.f, 774.f, 980.f};
        for (int i = 0; i < 5; ++i) {
            float bw = ps(194.f), bh = ps(64.f);
            auto itemNode = CCNode::create();
            itemNode->setContentSize({bw, bh});

            if (auto icoSpr = ui::createBottomBarIconSprite(i, ccp(ps(28.f), bh * 0.5f))) {
                itemNode->addChild(icoSpr, 1);
            }
            if (auto lbl = ui::createStyledLabel(BOTTOM_NAMES[i], true, 0.22f, pal.text, bw - ps(52.f))) {
                lbl->setAnchorPoint({0.f, 0.5f});
                lbl->setPosition({ps(46.f), bh * 0.5f});
                itemNode->addChild(lbl, 2);
            }

            auto btn = CCMenuItemExt::createSpriteExtra(
                itemNode, [this, i](CCMenuItemSpriteExtra*) { this->onBottomBarAction(i); }
            );
            btn->m_scaleMultiplier = 1.03f;
            btn->setPosition({px(btnCentersX[i]), py(942.f)});
            m_fixedMenu->addChild(btn);
        }

        // Banner inferior derecho del robot aliado (x_ref=1108..1508, y_top_ref=894..990)
        m_allyBgNode = CCDrawNode::create();
        ui::drawRoundedRect(
            m_allyBgNode,
            px(1108.f), py(990.f), ps(400.f), ps(96.f), ps(18.f),
            rgba4F(pal.panel, 0.98f), 0.f, ccc4f(0, 0, 0, 0)
        );
        ui::drawRoundedRect(
            m_allyBgNode,
            px(1126.f), py(968.f), ps(52.f), ps(52.f), ps(13.f),
            rgba4F(pal.elevated, 0.98f), ps(1.0f), rgba4F(pal.lineStrong, 0.9f)
        );
        m_mainLayer->addChild(m_allyBgNode, 3);

        m_allyBorderNode = CCDrawNode::create();
        m_mainLayer->addChild(m_allyBorderNode, 4);

        m_allyRobotNode = ui::createRobotMascotNode(false);
        if (auto face = typeinfo_cast<CCSprite*>(m_allyRobotNode->getChildByTag(301)))
            m_allyRobotBaseScale = face->getScale();
        m_allyRobotNode->ignoreAnchorPointForPosition(false);
        m_allyRobotNode->setAnchorPoint({0.5f, 0.5f});
        m_allyRobotNode->setPosition({px(1152.f), py(942.f)});
        m_mainLayer->addChild(m_allyRobotNode, 6);

        m_allyLine1 = ui::createStyledLabel("Cada grupo libre es una oportunidad.", false, 0.40f, pal.text2, ps(320.f));
        if (m_allyLine1) {
            m_allyLine1->setAnchorPoint({0.f, 0.5f});
            m_allyLine1->setPosition({px(1180.f), py(942.f)});
            m_mainLayer->addChild(m_allyLine1, 6);
        }
    }

    void nextAllyPhrase() {
        if (m_phrases.empty()) m_phrases = HistoryManager::defaultPhrases();
        m_phraseIdx = (m_phraseIdx + 1) % m_phrases.size();
        m_phraseTimer = 0.f;
        this->updateAllyBannerText();
    }

    void updateAllyBannerText() {
        if (!m_allyLine1) return;
        if (m_phrases.empty()) m_phrases = HistoryManager::defaultPhrases();
        std::string phrase = m_phrases[m_phraseIdx % m_phrases.size()];
        fitBMFontLabel(m_allyLine1, phrase, ps(304.f), 0.40f);
    }

    void refreshUsagePanel() {
        auto pal = ThemeConfig::getPalette();
        int used = Mod::get()->getSavedValue<int>("used", 0);
        int limit = std::max(1, Mod::get()->getSavedValue<int>("limit", 20));
        int total = Mod::get()->getSavedValue<int>("total", 0);
        EditorContextSnapshot ctx = LevelExecutor::scanEditorContext(m_lel);

        if (m_quotaBigNumLbl) fitBMFontLabel(m_quotaBigNumLbl, fmt::format("{}", used), ps(90.f), 0.54f, true);
        if (m_quotaSubLbl) fitBMFontLabel(m_quotaSubLbl, fmt::format("/ {} peticiones hoy", limit), ps(260.f), 0.46f);
        if (m_reqDoneValLbl) fitBMFontLabel(m_reqDoneValLbl, fmt::format("{}", total), ps(100.f), 0.26f, true);
        if (m_curLevelValLbl) {
            fitBMFontLabel(
                m_curLevelValLbl,
                fmt::format("{} · {} obj", LevelExecutor::getLevelTitle(m_lel), ctx.totalObjects),
                ps(210.f), 0.24f, true
            );
        }

        float frac = std::clamp(static_cast<float>(used) / static_cast<float>(limit), 0.f, 1.f);
        if (m_quotaBarFill) {
            m_quotaBarFill->clear();
            ui::drawRoundedRect(
                m_quotaBarFill,
                px(1132.f), py(256.f), ps(352.f * frac), ps(12.f), ps(6.f),
                rgba4F(pal.accent, 1.f), 0.f, ccc4f(0, 0, 0, 0)
            );
        }
    }

    void refreshHistoryMiniPanel() {
        auto const& h = HistoryManager::entries();
        for (int r = 0; r < 3; ++r) {
            int k = static_cast<int>(h.size()) - 1 - r;
            std::string pStr = (k >= 0) ? h[k].summary : (r == 0 && h.empty() ? "Sin historial todavia" : "");
            std::string tStr = (k >= 0) ? HistoryManager::formatRelativeTime(h[k].timestamp) : "";

            if (m_histRowText[r]) fitBMFontLabel(m_histRowText[r], pStr, ps(200.f), 0.36f);
            if (m_histRowTime[r]) fitBMFontLabel(m_histRowTime[r], tStr, ps(76.f), 0.37f);
            if (m_chatHistRowText[r]) fitBMFontLabel(m_chatHistRowText[r], pStr, ps(190.f), 0.36f);
            if (m_chatHistRowTime[r]) fitBMFontLabel(m_chatHistRowTime[r], tStr, ps(76.f), 0.37f);
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
                std::string curPrompt = m_promptInput ? sanitizeForGD(std::string(m_promptInput->getString().c_str())) : "";
                if (auto pop = FavoritesPopup::create(
                    curModeId, curPrompt,
                    [this](int mi, std::string const& p) {
                        if (mi >= 0) this->selectMode(mi);
                        this->setPromptText(p);
                        this->showPage(2);
                    }
                )) {
                    pop->show();
                }
                break;
            }
            case 1: {
                if (auto pop = TutorialsPopup::create()) pop->show();
                break;
            }
            case 2: {
                if (auto pop = LevelsBackupPopup::create(m_lel)) pop->show();
                break;
            }
            case 3: {
                std::string msg;
                bool ok = LevelExecutor::saveLevelSnapshot(m_lel, msg);
                FLAlertLayer::create(ok ? "Guardado Completado" : "Aviso", msg, "OK")->show();
                if (m_activePage == 3) this->refreshPage3Stats();
                break;
            }
            default: {
                if (m_aiClient.isBusy()) m_aiClient.cancel();
                this->setPromptText("");
                this->selectMode(0);
                this->showPage(0);
                break;
            }
        }
    }

    void updateChatStatusText(char const* statusTitle, ccColor3B statusCol, std::string const& body) {
        m_lastReplyText = sanitizeForGD(body);
        if (std::string(statusTitle) != "PROCESANDO" && std::string(statusTitle) != "ENVIANDO") {
            HistoryManager::addChatMessage(false, m_lastReplyText);
            this->refreshConversation();
        }
        if (m_chatStateBadgeLbl) {
            fitBMFontLabel(m_chatStateBadgeLbl, statusTitle, ps(70.f), 0.20f, true);
            m_chatStateBadgeLbl->setColor(statusCol);
        }
        if (m_chatAiBubbleLbl) {
            fitBMFontLabel(m_chatAiBubbleLbl, m_lastReplyText, ps(668.f), 0.43f);
        }
    }

    void onSendCommand() {
        if (m_aiClient.isBusy()) {
            this->showPage(2);
            return;
        }

        if (m_serverUrlInput) {
            std::string liveUrl = trimStr(std::string(m_serverUrlInput->getString().c_str()));
            if (!liveUrl.empty()) AIClient::setServerUrl(liveUrl);
        }

        std::string prompt = m_promptInput ? trimStr(sanitizeForGD(std::string(m_promptInput->getString().c_str()))) : "";
        if (prompt.empty()) {
            this->showPage(2);
            this->updateChatStatusText(
                "AVISO",
                COL_STATUS_AMBER,
                "Escribe primero una instruccion en la caja de texto o toca uno de los chips rapidos antes de enviar."
            );
            return;
        }

        m_lastUserBubbleText = prompt;
        HistoryManager::addChatMessage(true, prompt);
        this->refreshConversation();
        if (m_chatUserBubbleLbl) {
            fitBMFontLabel(m_chatUserBubbleLbl, m_lastUserBubbleText, ps(516.f), 0.44f);
        }

        if (prompt == "/catalogo") {
            this->setPromptText("");
            this->showPage(2);
            this->updateChatStatusText(
                "ENVIANDO",
                COL_STATUS_AMBER,
                "Leyendo el catalogo de objetos del juego y subiendolo al servidor..."
            );
            m_aiClient.uploadCatalog([this](bool ok, std::string const& msg) {
                this->updateChatStatusText(
                    ok ? "LISTO" : "REVISION",
                    ok ? COL_STATUS_GREEN : COL_STATUS_RED,
                    msg
                );
            });
            return;
        }

        int modeToUse = (m_selectedMode >= 0 && m_selectedMode < 9) ? m_selectedMode : 1;
        this->setPromptText("");
        this->showPage(2);

        this->updateChatStatusText(
            "PROCESANDO",
            COL_STATUS_AMBER,
            fmt::format("Procesando en modo {}: \"{}\"...", MODES[modeToUse].name, prompt)
        );

        m_aiClient.sendRequest(
            modeToUse,
            prompt,
            m_lel,
            [this, modeToUse](AIResponseData const& res) {
                if (!res.ok) {
                    m_serverReachable = false;
                    m_serverChecked = true;
                    this->refreshHeaderStatusPill();
                    this->updateChatStatusText("REVISION", COL_STATUS_RED, res.message);
                    this->refreshUsagePanel();
                    return;
                }

                HistoryManager::addEntry(MODES[modeToUse].id, m_lastUserBubbleText);
                this->refreshHistoryMiniPanel();
                ++m_sessionRequests;
                Mod::get()->setSavedValue<int>("total", Mod::get()->getSavedValue<int>("total", 0) + 1);

                if (AIClient::getOriginMode() == "server") { m_serverReachable = true; m_serverChecked = true; }
                this->refreshHeaderStatusPill();
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
                        m_canUndoAiAction = true;
                        m_sessionPlacedObjects += vr.validCount;
                        m_lastSummaryPill1 = fmt::format("{} objetos", vr.validCount);
                        m_lastSummaryPill2 = std::string("Modo ") + MODES[modeToUse].name;
                        if (m_chatPillLbls[0]) fitBMFontLabel(m_chatPillLbls[0], m_lastSummaryPill1, ps(112.f), 0.37f);
                        if (m_chatPillLbls[1]) fitBMFontLabel(m_chatPillLbls[1], m_lastSummaryPill2, ps(118.f), 0.37f);
                        finalMsg += fmt::format(" [{} objetos colocados].", vr.validCount);
                    } else if (!vr.note.empty()) {
                        finalMsg += " [" + vr.note + "]";
                    }
                }

                this->updateChatStatusText("LISTO", COL_STATUS_GREEN, finalMsg);
                this->refreshUsagePanel();
                this->refreshPage2ChatPerception();
            }
        );

        this->refreshUsagePanel();
    }

    // Bucle de animacion: Flujo RGB en tarjeta seleccionada, picos y robot, mas transicion de carga de fuente
    void tick(float dt) {
        if (m_pendingRebuild) {
            m_pendingRebuild = false;
            this->rebuildAllUIContent();
            return;
        }
        m_animTime += dt;
        m_connectionCheckTimer += dt;
        if (m_connectionCheckTimer >= 15.f && AIClient::getOriginMode() == "server") {
            m_connectionCheckTimer = 0.f;
            m_aiClient.checkConnection([this](bool connected) {
                m_serverChecked = true;
                m_serverReachable = connected;
                this->refreshHeaderStatusPill();
            });
        }
        float rgbSpeed = ThemeConfig::getRgbSpeed();
        m_wormPhase += dt * 0.42f * rgbSpeed;

        // Si esta activa la pantalla de "Cargando fuente...", actualizar cuenta regresiva y aplicar fuente al terminar
        if (m_fontLoadingOverlay && m_pendingFontIndex >= 0) {
            m_fontLoadingRemaining -= dt;
            if (m_fontLoadingRemaining <= 0.f) {
                int targetFont = m_pendingFontIndex;
                m_pendingFontIndex = -1;
                ThemeConfig::setFontIndex(targetFont);
                this->rebuildAllUIContent();
                return;
            }
        }

        if (m_mascotTop) {
            m_mascotTop->setPositionY(m_mascotTop->getContentSize().height * 0.5f + std::sin(m_animTime * 2.4f) * 1.4f);
            if (auto face = typeinfo_cast<CCSprite*>(m_mascotTop->getChildByTag(301)))
                face->setScale(m_headerRobotBaseScale * (1.f + .012f * std::sin(m_animTime * 2.8f)));
        }
        if (m_allyRobotNode) {
            m_allyRobotNode->setPosition({
                px(1152.f),
                py(942.f) + std::sin(m_animTime * 3.6f) * 1.4f
            });
            if (auto face = typeinfo_cast<CCSprite*>(m_allyRobotNode->getChildByTag(301)))
                face->setScale(m_allyRobotBaseScale * (1.f + .035f * std::sin(m_animTime * 3.6f)));
        }

        auto pal = ThemeConfig::getPalette();
        bool rgbOn = ThemeConfig::isRgbEnabled();
        const float cardW = ps(264.f);
        const float cardH = ps(214.f);

        // 1. Estela RGB en las tarjetas de INICIO (enfatizada en la tarjeta seleccionada como en Pagina 1)
        for (int i = 0; i < 9; ++i) {
            if (!m_cardBorderNodes[i]) continue;
            if (i == m_selectedMode) {
                ui::drawWormPerimeterBorder(
                    m_cardBorderNodes[i],
                    0.f, 0.f, cardW, cardH, ps(16.f),
                    pal.accent,
                    m_wormPhase,
                    rgbOn,
                    false,
                    ps(2.6f)
                );
            } else {
                m_cardBorderNodes[i]->clear();
            }
        }

        // 2. Picos decorativos de la esquina inferior izquierda con su color Pinchos RGB
        ui::redrawSpikesWormRgb(m_spikesRgbNode, m_wormPhase, rgbOn);

        // 3. Contorno RGB alrededor del banner inferior derecho del robot aliado
        ccColor3B robotCol = ThemeConfig::getRobotRgbColor();
        ui::drawWormPerimeterBorder(
            m_allyBorderNode,
            px(1108.f), py(990.f), ps(400.f), ps(96.f), ps(18.f),
            robotCol,
            m_wormPhase + 0.35f,
            rgbOn,
            false,
            ps(2.2f)
        );

        // 4. Rotacion suave de frases del robot aliado
        const float cycleDuration = 6.5f;
        m_phraseTimer += dt;
        if (m_phraseTimer >= cycleDuration) {
            m_phraseTimer = 0.f;
            if (!m_phrases.empty()) {
                m_phraseIdx = (m_phraseIdx + 1) % m_phrases.size();
                this->updateAllyBannerText();
            }
        }
    }

public:
    ~DecoMindPopup() override {
        m_promptFieldDelegate.unbind();
    }

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

        auto win = CCDirector::get()->getWinSize();
        auto btn = DragButton::create(makeEditorButtonVisual(), [this] {
            this->onOpenDecoMind(nullptr);
        });
        if (btn) {
            auto sz = btn->getContentSize();
            float halfW = sz.width * 0.5f;
            float halfH = sz.height * 0.5f;
            float savedX = Mod::get()->getSavedValue<float>("btn-x", win.width * 0.34f);
            float savedY = Mod::get()->getSavedValue<float>("btn-y", win.height - 28.f);
            btn->setPosition({
                std::clamp(savedX, halfW, win.width - halfW),
                std::clamp(savedY, halfH, win.height - halfH)
            });
            btn->setID("decomind-menu"_spr);
            this->addChild(btn, 100);
        }
        return true;
    }

    void onOpenDecoMind(CCObject*) {
        if (auto popup = DecoMindPopup::create(m_editorLayer)) {
            popup->show();
        }
    }
};
