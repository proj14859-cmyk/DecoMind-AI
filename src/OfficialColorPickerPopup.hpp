#pragma once
// ===========================================================================
// DecoMind AI v0.3.4-alpha - Selector de Color Oficial de Geometry Dash
//
// Reemplazo directo de GDColorPickerPopup (misma firma de create()) basado en
// la ingenieria inversa de libcocos2dcpp.so (ARM64):
//   - Usa cocos2d::extension::CCControlColourPicker::colourPicker() (0xa9cf2c),
//     que carga CCControlColourPickerSpriteSheet.png con CCControlHuePicker y
//     CCControlSaturationBrightnessPicker (prioridad tactil -500 con ForcePrio).
//   - Corrige el pivote de escalado (setAnchorPoint({0.f, 0.f})) ya que los
//     sprites internos de CCControlColourPicker estan centrados en (0, 0) con
//     ignoreAnchorPointForPosition(true).
//   - Incluye vistas previas de color actual y original (whiteSquare60_001.png),
//     entradas sincronizadas R:, G:, B: y HEX: con guarda anti-recursion
//     (m_updatingInputs, equivalente a m_disableTextDelegate en 0x74046c),
//     botones COPY, PASTE, DEFAULT y boton OK.
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
        if (!this->initModal(360.f, 246.f, "", false)) return false;
        m_cur = initialCol;
        m_initial = initialCol;
        m_default = defaultCol;
        m_onApply = std::move(onApply);

        // 1. Vistas previas de color (actual arriba y color original debajo, como en ColorSelectPopup 0x73db48)
        m_previewCurrent = CCSprite::createWithSpriteFrameName("whiteSquare60_001.png");
        m_previewOrig = CCSprite::createWithSpriteFrameName("whiteSquare60_001.png");
        if (m_previewCurrent && m_previewOrig) {
            m_previewCurrent->setScale(0.58f);
            m_previewCurrent->setColor(m_cur);
            m_previewCurrent->setPosition({34.f, 212.f});
            m_mainLayer->addChild(m_previewCurrent, 6);

            m_previewOrig->setScale(0.58f * 0.5f);
            m_previewOrig->setColor(m_initial);
            m_previewOrig->setPosition({34.f, 186.f});
            m_mainLayer->addChild(m_previewOrig, 6);
        } else {
            m_fallbackPreviewDn = CCDrawNode::create();
            m_mainLayer->addChild(m_fallbackPreviewDn, 6);
        }

        // 2. Entradas R:, G:, B: y HEX: a la izquierda (sincronizadas en vivo y sin bucle recursivo)
        char const* rgbTitles[3] = {"R:", "G:", "B:"};
        float rgbX[3] = {28.f, 66.f, 104.f};
        auto leftBgDn = CCDrawNode::create();
        for (int i = 0; i < 3; ++i) {
            auto tl = CCLabelBMFont::create(rgbTitles[i], "bigFont.fnt");
            tl->setScale(0.28f);
            tl->setColor(ccc3(255, 230, 120));
            tl->setPosition({rgbX[i], 164.f});
            m_mainLayer->addChild(tl, 6);

            ui::drawRoundedRect(
                leftBgDn, rgbX[i] - 16.f, 134.f, 32.f, 20.f, 4.f,
                rgba4F(ccc3(0x04, 0x0e, 0x28), 0.95f), 0.8f, rgba4F(COL_DIVIDER, 0.9f)
            );

            auto inp = TextInput::create(30.f, "255", "bigFont.fnt");
            inp->setFilter("0123456789");
            inp->setMaxCharCount(3);
            inp->setScale(0.85f);
            inp->setPosition({rgbX[i], 144.f});
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

        auto hexTitle = CCLabelBMFont::create("HEX:", "bigFont.fnt");
        hexTitle->setScale(0.28f);
        hexTitle->setColor(ccc3(255, 230, 120));
        hexTitle->setPosition({66.f, 116.f});
        m_mainLayer->addChild(hexTitle, 6);

        ui::drawRoundedRect(
            leftBgDn, 24.f, 84.f, 84.f, 22.f, 4.f,
            rgba4F(ccc3(0x04, 0x0e, 0x28), 0.95f), 0.8f, rgba4F(COL_DIVIDER, 0.9f)
        );
        m_mainLayer->addChild(leftBgDn, 4);

        m_hexInput = TextInput::create(78.f, "FFFFFF", "bigFont.fnt");
        m_hexInput->setFilter("0123456789abcdefABCDEF");
        m_hexInput->setMaxCharCount(6);
        m_hexInput->setPosition({66.f, 95.f});
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

        // 3. Rueda nativa oficial de Geometry Dash (CCControlColourPicker)
        m_wheel = cocos2d::extension::CCControlColourPicker::colourPicker();
        if (m_wheel) {
            // En CCControlColourPicker::init() (0xa9cb8c), los sprites hijos se centran en (0, 0)
            // mientras que contentSize es (168, 168) e ignoreAnchorPointForPosition es true.
            // Fijar anchorPoint en (0, 0) garantiza que setScale escale exactamente desde el centro (0, 0)
            // sin desplazar la rueda ni desalinear convertToNodeSpace en los toques.
            m_wheel->setAnchorPoint({0.f, 0.f});
            m_wheel->setPosition({188.f, 134.f});
            m_wheel->setScale(0.84f);
            m_wheel->setColorValue(m_cur);
            m_wheel->setDelegate(this);
            m_mainLayer->addChild(m_wheel, 10);
        }

        // 4. Botones derechos: COPY, PASTE, DEFAULT (como en ColorSelectPopup 0x73d8bc..0x73d918)
        auto copySpr = this->makeStyledActionBtn("COPY", 62.f, 24.f, ccc3(0x16, 0x28, 0x4c), COL_TEXT_SOFT);
        auto copyBtn = CCMenuItemExt::createSpriteExtra(copySpr, [this](CCMenuItemSpriteExtra*) {
            ThemeConfig::setClipboardColor(m_cur);
        });
        copyBtn->m_scaleMultiplier = 1.03f;
        copyBtn->setPosition({316.f, 208.f});
        m_ui->addChild(copyBtn);

        auto pasteSpr = this->makeStyledActionBtn("PASTE", 62.f, 24.f, ccc3(0x16, 0x28, 0x4c), COL_TEXT_SOFT);
        auto pasteBtn = CCMenuItemExt::createSpriteExtra(pasteSpr, [this](CCMenuItemSpriteExtra*) {
            ccColor3B clip = ThemeConfig::getClipboardColor();
            this->applyColorProgrammatically(clip);
        });
        pasteBtn->m_scaleMultiplier = 1.03f;
        pasteBtn->setPosition({316.f, 174.f});
        m_ui->addChild(pasteBtn);

        auto defSpr = this->makeStyledActionBtn("DEFAULT", 62.f, 24.f, ccc3(0x16, 0x28, 0x4c), COL_TEXT_SOFT);
        auto defBtn = CCMenuItemExt::createSpriteExtra(defSpr, [this](CCMenuItemSpriteExtra*) {
            this->applyColorProgrammatically(m_default);
        });
        defBtn->m_scaleMultiplier = 1.03f;
        defBtn->setPosition({316.f, 140.f});
        m_ui->addChild(defBtn);

        // 5. Boton inferior central: OK
        auto okSpr = this->makeStyledActionBtn("OK", 68.f, 26.f, ccc3(0x10, 0x5c, 0x38), COL_NEON_GREEN);
        auto okBtn = CCMenuItemExt::createSpriteExtra(okSpr, [this](CCMenuItemSpriteExtra*) {
            if (m_onApply) m_onApply(m_cur);
            this->onClose(nullptr);
        });
        okBtn->m_scaleMultiplier = 1.03f;
        okBtn->setPosition({188.f, 24.f});
        m_ui->addChild(okBtn);

        // Sincronizacion inicial de todos los campos
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
                m_fallbackPreviewDn, 18.f, 192.f, 32.f, 36.f, 3.f,
                rgba4F(c, 1.f), 1.5f, rgba4F(COL_TEXT_WHITE, 0.95f)
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
    // Callback de ColorPickerDelegate disparado por CCControlColourPicker al arrastrar
    // el anillo de matiz (hueSliderValueChanged) o el disco S/V (colourSliderValueChanged).
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
