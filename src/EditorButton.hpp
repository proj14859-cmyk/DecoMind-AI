#pragma once
// ===========================================================================
// DecoMind AI v0.3.6-alpha - Boton Flotante y Movible del Editor
//
// Incluye soporte para el control de Opacidad (20%..100%, por defecto 55%)
// y el boton "RESTABLECER POSICION" de la pestana Ajustes (Pagina 5 del PDF),
// ademas del contorno circular fino (~0.85pt) recoloreado segun la paleta.
// ===========================================================================
#include <Geode/Geode.hpp>
#include <algorithm>
#include <cmath>
#include <functional>
#include "UIRenderer.hpp"

using namespace geode::prelude;

namespace decomind {

inline void drawThinEditorButtonRing(CCDrawNode* dn, float D, float alpha) {
    if (!dn) return;
    dn->clear();

    auto pal = ThemeConfig::getPalette();
    CCPoint center = ccp(D * 0.5f, D * 0.5f);
    float rOuter = D * 0.5f - 0.6f;
    float rInner = rOuter - 0.85f; // Contorno delgado (~0.85pt) pero claramente visible

    ccColor3B accent = lerpColor(pal.accent, pal.accentSoft, 0.35f);
    float borderAlpha = std::min(1.f, alpha + 0.30f);
    float fillAlpha = std::min(1.f, alpha * 0.90f);

    ui::drawCircleShape(
        dn,
        center,
        rOuter,
        rgba4F(accent, borderAlpha),
        0.f,
        ccc4f(0.f, 0.f, 0.f, 0.f),
        48
    );

    ui::drawCircleShape(
        dn,
        center,
        rInner,
        rgba4F(pal.bgOuter, fillAlpha),
        0.f,
        ccc4f(0.f, 0.f, 0.f, 0.f),
        48
    );
}

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

    static constexpr GLubyte ACTIVE_OPACITY = 235;
    static constexpr float DRAG_THRESHOLD_PX = 6.5f;

    GLubyte getIdleOpacityByte() const {
        int pct = ThemeConfig::getEditorBtnOpacityPct();
        return static_cast<GLubyte>(std::clamp((pct * 255) / 100, 40, 255));
    }

    bool isHierarchyVisible() {
        for (CCNode* node = this; node != nullptr; node = node->getParent()) {
            if (!node->isVisible()) return false;
        }
        return true;
    }

    bool isModalOrPlaytestActive() {
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
        float r = std::max(cx, cy) + 3.f;
        float dx = local.x - cx;
        float dy = local.y - cy;
        return (dx * dx + dy * dy) <= (r * r);
    }

    void drawRingWithAlpha(float alpha) {
        if (!m_ring) return;
        float D = this->getContentSize().width;
        drawThinEditorButtonRing(m_ring, D, alpha);
    }

    void setVisualFeedback(bool active, bool dragging) {
        GLubyte idleByte = this->getIdleOpacityByte();
        GLubyte curByte = active ? std::max(idleByte, ACTIVE_OPACITY) : idleByte;
        float alpha = curByte / 255.f;
        this->drawRingWithAlpha(alpha);
        if (m_iconRgba) {
            m_iconRgba->setOpacity(curByte);
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

    void resetToDefaultPosition() {
        auto win = CCDirector::get()->getWinSize();
        float defX = win.width * 0.34f;
        float defY = win.height - 28.f;
        this->setPosition({defX, defY});
        Mod::get()->setSavedValue<float>("btn-x", defX);
        Mod::get()->setSavedValue<float>("btn-y", defY);
        (void)Mod::get()->saveData();
        this->refreshTheme();
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

            r->setTouchMode(kCCTouchesOneByOne);
            r->setTouchEnabled(true);
            r->setVisualFeedback(false, false);
            return r;
        }
        CC_SAFE_DELETE(r);
        return nullptr;
    }

    void registerWithTouchDispatcher() override {
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
            (void)Mod::get()->saveData();
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
    constexpr float D = 34.f;
    int pct = ThemeConfig::getEditorBtnOpacityPct();
    GLubyte idleByte = static_cast<GLubyte>(std::clamp((pct * 255) / 100, 40, 255));
    float idleFloat = idleByte / 255.f;

    auto btnVisual = CCNode::create();
    btnVisual->setContentSize({D, D});
    btnVisual->ignoreAnchorPointForPosition(false);
    btnVisual->setAnchorPoint({0.5f, 0.5f});

    auto ring = CCDrawNode::create();
    ring->setTag(101);
    drawThinEditorButtonRing(ring, D, idleFloat);
    btnVisual->addChild(ring, 1);

    if (auto icon = CCSprite::create("editor-btn.png"_spr)) {
        icon->setTag(102);
        icon->setScale((D * 0.70f) / std::max(icon->getContentWidth(), icon->getContentHeight()));
        icon->setOpacity(idleByte);
        icon->setPosition({D * 0.5f, D * 0.5f});
        btnVisual->addChild(icon, 2);
    } else {
        auto lbl = CCLabelBMFont::create("IA", "bigFont.fnt");
        lbl->setTag(102);
        lbl->setScale(0.45f);
        lbl->setOpacity(idleByte);
        lbl->setPosition({D * 0.5f, D * 0.5f});
        btnVisual->addChild(lbl, 2);
    }

    return btnVisual;
}

} // namespace decomind
