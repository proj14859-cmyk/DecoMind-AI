#pragma once
// ===========================================================================
// DecoMind AI v0.3.4-alpha - Boton Flotante y Movible del Editor
//
// Corroborado y corregido contra libcocos2dcpp.so (ARM64):
//   1. Activa setTouchEnabled(true) y setTouchMode(kCCTouchesOneByOne) en init;
//      en CCLayer::init() (0xaea00c) m_bTouchEnabled inicia en false y sin esto
//      CCLayer::onEnter() (0xaec3ec) jamas invoca registerWithTouchDispatcher().
//   2. Desactiva ignoreAnchorPointForPosition(false) y fija anchorPoint en
//      {0.5f, 0.5f} para que setPosition() posicione el centro real del boton
//      y coincida con el clamp de bordes de pantalla.
//   3. Prioridad tactil -250: superior al EditorUI (0) y a las barras CCMenu
//      del editor (-128), pero inferior a los modales FLAlertLayer / Popup
//      (-501 o menor con ForcePrio), evitando robar toques a traves de ventanas
//      abiertas o durante el modo Playtest.
//   4. Corrige el namespace decomind::rgba4F (antes llamado como ui::rgba4F).
//   5. Transparencia del ~55% en reposo (opacidad 140/255, dentro del rango
//      50%-60%), aumentando suavemente al mantener pulsado/arrastrar.
//   6. Comportamiento dual:
//      - Picar una vez (toque corto sin desplazar) -> abre el menu DecoMind AI.
//      - Mantener pulsado y mover el dedo o el raton -> arrastra el boton por
//        toda la pantalla y guarda la posicion en "btn-x" y "btn-y".
// ===========================================================================
#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <functional>
#include "UIRenderer.hpp"

using namespace geode::prelude;

namespace decomind {

class DragButton : public CCLayer {
protected:
    CCNode* m_visual = nullptr;
    CCDrawNode* m_ring = nullptr;
    CCRGBAProtocol* m_iconRgba = nullptr;
    std::function<void()> m_onTap;
    CCPoint m_startTouch{0.f, 0.f};
    CCPoint m_startPos{0.f, 0.f};
    bool m_down = false;
    bool m_dragging = false;

    static constexpr GLubyte IDLE_OPACITY = 140;   // ~55% opacidad (transparencia entre 50% y 60%)
    static constexpr GLubyte ACTIVE_OPACITY = 225; // ~88% opacidad al mantener pulsado / arrastrar
    static constexpr float DRAG_THRESHOLD_PX = 6.5f;

    bool isHierarchyVisible() const {
        for (CCNode const* node = this; node != nullptr; node = node->getParent()) {
            if (!node->isVisible()) return false;
        }
        return true;
    }

    bool isModalOrPlaytestActive() const {
        if (auto lel = LevelEditorLayer::get()) {
            if (lel->m_playbackMode == PlaybackMode::Playing) {
                return true;
            }
        }
        if (auto scene = CCDirector::get()->getRunningScene()) {
            if (auto children = scene->getChildren()) {
                for (unsigned int i = 0; i < children->count(); ++i) {
                    if (typeinfo_cast<FLAlertLayer*>(children->objectAtIndex(i))) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool hit(CCTouch* t) {
        if (!t) return false;
        auto local = this->convertToNodeSpace(t->getLocation());
        auto sz = this->getContentSize();
        float cx = sz.width * 0.5f;
        float cy = sz.height * 0.5f;
        float r = std::max(cx, cy) + 3.f; // Margen tactil comodo en movil y raton
        float dx = local.x - cx;
        float dy = local.y - cy;
        return (dx * dx + dy * dy) <= (r * r);
    }

    void drawRingWithAlpha(float alpha) {
        if (!m_ring) return;
        m_ring->clear();
        float D = this->getContentSize().width;
        ui::drawCircleShape(
            m_ring,
            ccp(D * 0.5f, D * 0.5f),
            D * 0.5f - 1.f,
            rgba4F(ccc3(0x04, 0x0e, 0x28), alpha),
            1.3f,
            rgba4F(ThemeConfig::getMenuAccent(), std::min(1.f, alpha + 0.12f)),
            36
        );
    }

    void setVisualFeedback(bool active, bool dragging) {
        float alpha = active ? (ACTIVE_OPACITY / 255.f) : (IDLE_OPACITY / 255.f);
        this->drawRingWithAlpha(alpha);
        if (m_iconRgba) {
            m_iconRgba->setOpacity(active ? ACTIVE_OPACITY : IDLE_OPACITY);
        }
        if (m_visual) {
            if (dragging) {
                m_visual->setScale(1.06f);
            } else if (active) {
                m_visual->setScale(0.92f);
            } else {
                m_visual->setScale(1.0f);
            }
        }
    }

public:
    void refreshTheme() {
        this->setVisualFeedback(m_down, m_dragging);
    }

    static DragButton* create(CCNode* visual, std::function<void()> onTap) {
        auto r = new DragButton();
        if (r && r->init()) {
            r->autorelease();
            r->m_visual = visual;
            r->m_onTap = std::move(onTap);

            auto sz = visual ? visual->getContentSize() : CCSize{34.f, 34.f};
            r->setContentSize(sz);
            r->ignoreAnchorPointForPosition(false);
            r->setAnchorPoint({0.5f, 0.5f});

            if (visual) {
                visual->ignoreAnchorPointForPosition(false);
                visual->setAnchorPoint({0.5f, 0.5f});
                visual->setPosition(sz / 2);
                r->addChild(visual);
                r->m_ring = typeinfo_cast<CCDrawNode*>(visual->getChildByTag(101));
                if (auto iconNode = visual->getChildByTag(102)) {
                    r->m_iconRgba = typeinfo_cast<CCRGBAProtocol*>(iconNode);
                }
            }

            // Indispensable en Cocos2d-x para que CCLayer::onEnter() llame a registerWithTouchDispatcher()
            r->setTouchMode(kCCTouchesOneByOne);
            r->setTouchEnabled(true);
            r->setVisualFeedback(false, false);
            return r;
        }
        CC_SAFE_DELETE(r);
        return nullptr;
    }

    void registerWithTouchDispatcher() override {
        // Prioridad -250: por encima de EditorUI (0) y CCMenu del editor (-128),
        // pero por debajo de cualquier popup FLAlertLayer (-501 o menor).
        CCTouchDispatcher::get()->addTargetedDelegate(this, -250, true);
    }

    bool ccTouchBegan(CCTouch* t, CCEvent*) override {
        if (!this->isHierarchyVisible() || this->isModalOrPlaytestActive() || !this->hit(t)) {
            return false;
        }
        m_down = true;
        m_dragging = false;
        m_startTouch = t->getLocation();
        m_startPos = this->getPosition();
        this->setVisualFeedback(true, false);
        return true;
    }

    void ccTouchMoved(CCTouch* t, CCEvent*) override {
        if (!m_down || !t) return;
        CCPoint d = t->getLocation() - m_startTouch;
        if (!m_dragging && ccpLength(d) > DRAG_THRESHOLD_PX) {
            m_dragging = true;
            this->setVisualFeedback(true, true);
        }
        if (!m_dragging) return;

        auto win = CCDirector::get()->getWinSize();
        auto sz = this->getContentSize();
        float halfW = sz.width * 0.5f;
        float halfH = sz.height * 0.5f;
        CCPoint targetLocal = {
            std::clamp(m_startPos.x + d.x, halfW, win.width - halfW),
            std::clamp(m_startPos.y + d.y, halfH, win.height - halfH)
        };
        this->setPosition(targetLocal);
    }

    void ccTouchEnded(CCTouch*, CCEvent*) override {
        if (!m_down) return;
        bool wasDragging = m_dragging;
        m_down = false;
        m_dragging = false;
        this->setVisualFeedback(false, false);

        if (wasDragging) {
            Mod::get()->setSavedValue<float>("btn-x", this->getPositionX());
            Mod::get()->setSavedValue<float>("btn-y", this->getPositionY());
        } else if (m_onTap) {
            m_onTap();
        }
    }

    void ccTouchCancelled(CCTouch*, CCEvent*) override {
        m_down = false;
        m_dragging = false;
        this->setVisualFeedback(false, false);
    }
};

inline CCNode* makeEditorButtonVisual() {
    // Boton circular flotante con ~55% de opacidad (50%-60% de transparencia)
    constexpr float D = 34.f;
    constexpr GLubyte IDLE_ALPHA_BYTE = 140; // ~55%
    constexpr float IDLE_ALPHA_FLOAT = IDLE_ALPHA_BYTE / 255.f;

    auto btnVisual = CCNode::create();
    btnVisual->setContentSize({D, D});
    btnVisual->ignoreAnchorPointForPosition(false);
    btnVisual->setAnchorPoint({0.5f, 0.5f});

    auto ring = CCDrawNode::create();
    ring->setTag(101);
    ui::drawCircleShape(
        ring,
        ccp(D * 0.5f, D * 0.5f),
        D * 0.5f - 1.f,
        rgba4F(ccc3(0x04, 0x0e, 0x28), IDLE_ALPHA_FLOAT),
        1.3f,
        rgba4F(ThemeConfig::getMenuAccent(), std::min(1.f, IDLE_ALPHA_FLOAT + 0.12f)),
        36
    );
    btnVisual->addChild(ring, 1);

    if (auto icon = CCSprite::create("editor-btn.png"_spr)) {
        icon->setTag(102);
        icon->setScale((D * 0.64f) / std::max(icon->getContentWidth(), icon->getContentHeight()));
        icon->setOpacity(IDLE_ALPHA_BYTE);
        icon->setPosition({D * 0.5f, D * 0.5f});
        btnVisual->addChild(icon, 2);
    } else {
        auto lbl = CCLabelBMFont::create("IA", "bigFont.fnt");
        lbl->setTag(102);
        lbl->setScale(0.45f);
        lbl->setOpacity(IDLE_ALPHA_BYTE);
        lbl->setPosition({D * 0.5f, D * 0.5f});
        btnVisual->addChild(lbl, 2);
    }

    return btnVisual;
}

} // namespace decomind
