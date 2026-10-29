#pragma once
// ===========================================================================
// DecoMind AI v0.3.7-alpha - Selector de Color Oficial de Geometry Dash
//
// Integra CCControlColourPicker con el diseno de la Pagina 5 del PDF:
//   - Muestra grande de color actual y original
//   - Casillas R, G, B y HEX sincronizadas sin recursion
//   - Botones COPY, PASTE, DEFAULT y barra inferior OK en color Acento
//     con texto en Tinta sobre acento.
// ===========================================================================
#include <Geode/Geode.hpp>
#include <Geode/ui/TextInput.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <string>

using namespace geode::prelude;

namespace decomind {

class OfficialColorPickerPopup : public DMModalBase, public cocos2d::extension::ColorPickerDelegate {
protected:
    cocos2d::extension::CCControlColourPicker* m_wheel = nullptr;
    CCSprite* m_previewCurrent = nullptr;
    CCSprite* m_previewOrig = nullptr;
    CCDrawNode* m_fallbackPreviewDn = nullptr;
    TextInput* m_rInput = nullptr;
    TextInput* m_gInput = nullptr;
    TextInput* m_bInput = nullptr;
    TextInput* m_hexInput = nullptr;

    ccColor3B m_cur{};
    ccColor3B m_initial{};
    ccColor3B m_default{};
    std::function<void(ccColor3B)> m_onApply;
    bool m_updatingInputs = false;

    bool init(ccColor3B initialCol, ccColor3B defaultCol, std::function<void(ccColor3B)> onApply) {
        if (!this->initModal(360.f, 246.f, "SELECTOR DE COLOR", true)) return false;
        m_cur = initialCol;
        m_initial = initialCol;
        m_default = defaultCol;
        m_onApply = std::move(onApply);

        auto pal = ThemeConfig::getPalette();

        m_previewCurrent = CCSprite::createWithSpriteFrameName("whiteSquare60_001.png");
        m_previewOrig = CCSprite::createWithSpriteFrameName("whiteSquare60_001.png");
        if (m_previewCurrent && m_previewOrig) {
            m_previewCurrent->setScale(0.54f);
            m_previewCurrent->setColor(m_cur);
            m_previewCurrent->setPosition({36.f, 192.f});
            m_mainLayer->addChild(m_previewCurrent, 6);

            m_previewOrig->setScale(0.28f);
            m_previewOrig->setColor(m_initial);
            m_previewOrig->setPosition({64.f, 184.f});
            m_mainLayer->addChild(m_previewOrig, 6);
        } else {
            m_fallbackPreviewDn = CCDrawNode::create();
            m_mainLayer->addChild(m_fallbackPreviewDn, 6);
        }

        char const* rgbTitles[3] = {"R", "G", "B"};
        float rgbX[3] = {28.f, 66.f, 104.f};
        auto leftBgDn = CCDrawNode::create();
        for (int i = 0; i < 3; ++i) {
            if (auto tl = ui::createStyledLabel(rgbTitles[i], false, 0.42f, pal.text3)) {
                tl->setPosition({rgbX[i], 158.f});
                m_mainLayer->addChild(tl, 6);
            }

            ui::drawRoundedRect(
                leftBgDn, rgbX[i] - 16.f, 128.f, 32.f, 20.f, 4.f,
                rgba4F(pal.bgOuter, 0.96f), 0.8f, rgba4F(pal.line, 0.9f)
            );

            auto inp = TextInput::create(30.f, "255", "bigFont.fnt");
            inp->setFilter("0123456789");
            inp->setMaxCharCount(3);
            inp->setScale(0.85f);
            inp->setPosition({rgbX[i], 138.f});
            if (inp->getBGSprite()) {
                inp->getBGSprite()->setOpacity(0);
            }
            inp->setCallback([this, i](std::string const& str) {
                if (m_updatingInputs || str.empty()) return;
                int val = std::clamp(static_cast<int>(std::strtol(str.c_str(), nullptr, 10)), 0, 255);
                if (i == 0) m_cur.r = static_cast<GLubyte>(val);
                else if (i == 1) m_cur.g = static_cast<GLubyte>(val);
                else m_cur.b = static_cast<GLubyte>(val);

                m_updatingInputs = true;
                if (m_wheel) m_wheel->setColorValue(m_cur);
                this->updatePreviews(m_cur);
                this->syncHexField(m_cur);
                m_updatingInputs = false;
            });
            m_mainLayer->addChild(inp, 12);
            if (i == 0) m_rInput = inp;
            else if (i == 1) m_gInput = inp;
            else m_bInput = inp;
        }

        if (auto hexTitle = ui::createStyledLabel("HEX", false, 0.42f, pal.text3)) {
            hexTitle->setPosition({66.f, 112.f});
            m_mainLayer->addChild(hexTitle, 6);
        }

        ui::drawRoundedRect(
            leftBgDn, 24.f, 80.f, 84.f, 22.f, 4.f,
            rgba4F(pal.bgOuter, 0.96f), 0.8f, rgba4F(pal.line, 0.9f)
        );
        m_mainLayer->addChild(leftBgDn, 4);

        m_hexInput = TextInput::create(78.f, "9B5CF6", "bigFont.fnt");
        m_hexInput->setFilter("0123456789abcdefABCDEF");
        m_hexInput->setMaxCharCount(6);
        m_hexInput->setPosition({66.f, 91.f});
        if (m_hexInput->getBGSprite()) {
            m_hexInput->getBGSprite()->setOpacity(0);
        }
        m_hexInput->setCallback([this](std::string const& str) {
            if (m_updatingInputs || str.size() != 6) return;
            unsigned int v = std::strtoul(str.c_str(), nullptr, 16);
            m_cur = ccc3(
                static_cast<GLubyte>((v >> 16) & 0xFF),
                static_cast<GLubyte>((v >> 8) & 0xFF),
                static_cast<GLubyte>(v & 0xFF)
            );
            m_updatingInputs = true;
            if (m_wheel) m_wheel->setColorValue(m_cur);
            this->updatePreviews(m_cur);
            this->syncRgbFields(m_cur);
            m_updatingInputs = false;
        });
        m_mainLayer->addChild(m_hexInput, 12);

        m_wheel = cocos2d::extension::CCControlColourPicker::colourPicker();
        if (m_wheel) {
            m_wheel->setAnchorPoint({0.f, 0.f});
            m_wheel->setPosition({188.f, 130.f});
            m_wheel->setScale(0.82f);
            m_wheel->setColorValue(m_cur);
            m_wheel->setDelegate(this);
            m_mainLayer->addChild(m_wheel, 10);
        }

        auto copySpr = ui::makeCustomMenuBtn("COPY", 62.f, 22.f, pal.card, pal.line, 0.24f);
        auto copyBtn = CCMenuItemExt::createSpriteExtra(copySpr, [this](CCMenuItemSpriteExtra*) {
            ThemeConfig::setClipboardColor(m_cur);
        });
        copyBtn->m_scaleMultiplier = 1.03f;
        copyBtn->setPosition({316.f, 188.f});
        m_ui->addChild(copyBtn);

        auto pasteSpr = ui::makeCustomMenuBtn("PASTE", 62.f, 22.f, pal.card, pal.line, 0.24f);
        auto pasteBtn = CCMenuItemExt::createSpriteExtra(pasteSpr, [this](CCMenuItemSpriteExtra*) {
            ccColor3B clip = ThemeConfig::getClipboardColor();
            this->applyColorProgrammatically(clip);
        });
        pasteBtn->m_scaleMultiplier = 1.03f;
        pasteBtn->setPosition({316.f, 156.f});
        m_ui->addChild(pasteBtn);

        auto defSpr = ui::makeCustomMenuBtn("DEFAULT", 62.f, 22.f, pal.card, pal.line, 0.24f);
        auto defBtn = CCMenuItemExt::createSpriteExtra(defSpr, [this](CCMenuItemSpriteExtra*) {
            this->applyColorProgrammatically(m_default);
        });
        defBtn->m_scaleMultiplier = 1.03f;
        defBtn->setPosition({316.f, 124.f});
        m_ui->addChild(defBtn);

        auto okSpr = ui::makeCustomMenuBtn("OK", 160.f, 26.f, pal.accent, pal.accent, 0.28f, true, 6.f);
        auto okBtn = CCMenuItemExt::createSpriteExtra(okSpr, [this](CCMenuItemSpriteExtra*) {
            if (m_onApply) m_onApply(m_cur);
            this->onClose(nullptr);
        });
        okBtn->m_scaleMultiplier = 1.03f;
        okBtn->setPosition({188.f, 24.f});
        m_ui->addChild(okBtn);

        m_updatingInputs = true;
        this->updatePreviews(m_cur);
        this->syncRgbFields(m_cur);
        this->syncHexField(m_cur);
        m_updatingInputs = false;

        return true;
    }

    void applyColorProgrammatically(ccColor3B c) {
        m_cur = c;
        m_updatingInputs = true;
        if (m_wheel) m_wheel->setColorValue(c);
        this->updatePreviews(c);
        this->syncRgbFields(c);
        this->syncHexField(c);
        m_updatingInputs = false;
    }

    void updatePreviews(ccColor3B c) {
        if (m_previewCurrent) {
            m_previewCurrent->setColor(c);
        }
        if (m_fallbackPreviewDn) {
            m_fallbackPreviewDn->clear();
            ui::drawRoundedRect(
                m_fallbackPreviewDn, 18.f, 176.f, 32.f, 32.f, 4.f,
                rgba4F(c, 1.f), 1.2f, rgba4F(COL_TEXT_WHITE, 0.9f)
            );
        }
    }

    void syncRgbFields(ccColor3B c) {
        if (m_rInput) m_rInput->setString(fmt::format("{}", static_cast<int>(c.r)));
        if (m_gInput) m_gInput->setString(fmt::format("{}", static_cast<int>(c.g)));
        if (m_bInput) m_bInput->setString(fmt::format("{}", static_cast<int>(c.b)));
    }

    void syncHexField(ccColor3B c) {
        if (m_hexInput) {
            m_hexInput->setString(fmt::format(
                "{:02X}{:02X}{:02X}",
                static_cast<int>(c.r),
                static_cast<int>(c.g),
                static_cast<int>(c.b)
            ));
        }
    }

public:
    void colorValueChanged(ccColor3B c) override {
        m_cur = c;
        this->updatePreviews(c);
        if (!m_updatingInputs) {
            m_updatingInputs = true;
            this->syncRgbFields(c);
            this->syncHexField(c);
            m_updatingInputs = false;
        }
    }

    static OfficialColorPickerPopup* create(
        ccColor3B initialCol,
        ccColor3B defaultCol,
        std::function<void(ccColor3B)> onApply
    ) {
        auto ret = new OfficialColorPickerPopup();
        if (ret->init(initialCol, defaultCol, std::move(onApply))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

} // namespace decomind
