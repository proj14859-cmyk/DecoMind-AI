#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/TextArea.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>

using namespace geode::prelude;

// Paleta (de tu imagen de referencia):
//   navy #061a32   panel #002d64   azul #1659e8   cian #30b7ee   claro #b2edff
// Nota: las fuentes de GD no dibujan bien tildes ni enes, por eso el texto va sin ellas.

struct ModeInfo {
    const char* id;
    const char* name;
    const char* desc;
};

static const ModeInfo MODES[9] = {
    {"estructurar",  "Estructurar",  "Organiza y estructura tu nivel de forma inteligente."},
    {"layout",       "Layout",       "Crea la estructura y la jugabilidad del nivel."},
    {"deco",         "Deco",         "Anade y mejora la decoracion del nivel."},
    {"layout_deco",  "Layout+Deco",  "Crea la jugabilidad y la decoracion al mismo tiempo."},
    {"block_design", "Block Design", "Disena bloques y estructuras personalizadas."},
    {"triggers",     "Triggers",     "Configura y usa triggers de forma automatica."},
    {"transiciones", "Transiciones", "Crea transiciones suaves y creativas."},
    {"efectos",      "Efectos",      "Anade efectos especiales, movimiento y mas."},
    {"ideas",        "Ideas",        "Obten ideas, mejoras y consejos para tu nivel."},
};

// ---------------------------------------------------------------------------
// Generador LOCAL (origen "local"): sin red, solo para probar.
// Formato de objeto de GD: "1,<id>,2,<x>,3,<y>;"  (id 1 = bloque, 8 = pico)
// ---------------------------------------------------------------------------
static std::string makeLocalLevel(int sections) {
    std::string s;
    auto obj = [&](int id, int x, int y) {
        s += fmt::format("1,{},2,{},3,{};", id, x, y);
    };

    int x = 300;
    for (int i = 0; i < sections; ++i) {
        switch (i % 4) {
            case 0:
                obj(8, x, 15);
                x += 240;
                break;
            case 1:
                obj(8, x, 15);
                obj(8, x + 30, 15);
                x += 270;
                break;
            case 2:
                obj(1, x, 15);
                obj(8, x + 90, 15);
                x += 270;
                break;
            case 3:
                obj(1, x, 15);
                obj(1, x + 30, 15);
                obj(1, x + 30, 45);
                obj(1, x + 60, 15);
                obj(1, x + 60, 45);
                obj(1, x + 60, 75);
                x += 330;
                break;
        }
    }
    return s;
}

// Busca una linea "CLAVE:valor" en la respuesta del servidor.
static std::string getField(std::string const& body, std::string const& key) {
    std::string prefix = key + ":";
    size_t pos = 0;
    while (pos < body.size()) {
        size_t end = body.find('\n', pos);
        if (end == std::string::npos) end = body.size();
        std::string line = body.substr(pos, end - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind(prefix, 0) == 0) return line.substr(prefix.size());
        pos = end + 1;
    }
    return "";
}

// Historial de la sesion (se conserva mientras el juego este abierto).
static std::vector<std::string>& history() {
    static std::vector<std::string> h;
    return h;
}

// ---------------------------------------------------------------------------
// Menu de DecoMind AI
// ---------------------------------------------------------------------------
class DecoMindPopup : public Popup {
protected:
    Ref<LevelEditorLayer> m_lel;
    int m_mode = 2; // Deco por defecto
    bool m_busy = false;
    std::vector<ButtonSprite*> m_modeSprites;
    TextInput* m_input = nullptr;
    CCLabelBMFont* m_modeLabel = nullptr;
    CCLabelBMFont* m_usageLabel = nullptr;
    CCLayerColor* m_barFill = nullptr;
    SimpleTextArea* m_desc = nullptr;
    SimpleTextArea* m_output = nullptr;
    CCLabelBMFont* m_hist[3] = {nullptr, nullptr, nullptr};
    async::TaskHolder<web::WebResponse> m_task;

    static constexpr float BAR_W = 178.f;

    CCLayerColor* rect(float x, float y, float w, float h, ccColor4B c) {
        auto l = CCLayerColor::create(c, w, h);
        l->setPosition({x, y});
        m_mainLayer->addChild(l, 1);
        return l;
    }

    CCLabelBMFont* label(const char* text, const char* font, float scale, CCPoint pos, ccColor3B col) {
        auto l = CCLabelBMFont::create(text, font);
        l->setScale(scale);
        l->setColor(col);
        l->setAnchorPoint({0.f, 0.5f});
        l->setPosition(pos);
        m_mainLayer->addChild(l, 10);
        return l;
    }

    bool init(LevelEditorLayer* lel) {
        if (!Popup::init(470.f, 290.f)) return false;
        m_lel = lel;

        // Fondo propio con la paleta azul (se oculta el fondo marron por defecto).
        m_bgSprite->setVisible(false);
        this->rect(0.f, 0.f, 470.f, 290.f, ccc4(0x30, 0xb7, 0xee, 255));
        this->rect(2.f, 2.f, 466.f, 286.f, ccc4(0x06, 0x1a, 0x32, 245));
        this->rect(10.f, 10.f, 244.f, 236.f, ccc4(0x00, 0x2d, 0x64, 210));
        this->rect(262.f, 10.f, 198.f, 236.f, ccc4(0x00, 0x2d, 0x64, 210));

        this->setTitle("DecoMind AI");
        if (m_title) m_title->setZOrder(10);

        // ---- Panel izquierdo: modos (3x3) ----
        for (int i = 0; i < 9; ++i) {
            int col = i % 3;
            int row = i / 3;
            auto spr = ButtonSprite::create(
                MODES[i].name, 76, true, "bigFont.fnt", "GJ_button_02.png", 30.f, .32f
            );
            m_modeSprites.push_back(spr);
            auto btn = CCMenuItemSpriteExtra::create(
                spr, this, menu_selector(DecoMindPopup::onMode)
            );
            btn->setTag(i);
            m_buttonMenu->addChildAtPosition(
                btn, Anchor::BottomLeft, ccp(51.f + col * 81.f, 222.f - row * 38.f)
            );
        }

        m_modeLabel = this->label("", "bigFont.fnt", .4f, ccp(16.f, 116.f), ccc3(0xb2, 0xed, 0xff));

        m_desc = SimpleTextArea::create(" ", "chatFont.fnt", .5f, 230.f);
        m_desc->setWrappingMode(WORD_WRAP);
        m_desc->setMaxLines(3);
        m_desc->setAnchorPoint({0.f, 1.f});
        m_desc->setPosition({16.f, 106.f});
        m_mainLayer->addChild(m_desc, 10);

        this->label("HISTORIAL", "bigFont.fnt", .35f, ccp(16.f, 66.f), ccc3(0x6a, 0xd2, 0xf5));
        for (int i = 0; i < 3; ++i) {
            m_hist[i] = this->label(
                "", "chatFont.fnt", .5f, ccp(16.f, 50.f - i * 14.f), ccc3(0xc7, 0xec, 0xff)
            );
        }

        // ---- Panel derecho: uso, chat y resultado ----
        this->label("USO DE LA IA", "bigFont.fnt", .4f, ccp(272.f, 232.f), ccc3(0x6a, 0xd2, 0xf5));
        this->rect(272.f, 210.f, BAR_W, 8.f, ccc4(0x01, 0x39, 0x68, 255));
        m_barFill = this->rect(272.f, 210.f, 0.01f, 8.f, ccc4(0x30, 0xb7, 0xee, 255));
        m_usageLabel = this->label("", "chatFont.fnt", .5f, ccp(272.f, 198.f), ccc3(0xc7, 0xec, 0xff));

        m_input = TextInput::create(178.f, "Escribe tu peticion...", "chatFont.fnt");
        m_input->setCommonFilter(CommonFilter::Any);
        m_input->setMaxCharCount(140);
        m_mainLayer->addChildAtPosition(m_input, Anchor::BottomLeft, ccp(361.f, 168.f));

        auto sendSpr = ButtonSprite::create(
            "Enviar", 100, true, "bigFont.fnt", "GJ_button_02.png", 30.f, .5f
        );
        auto sendBtn = CCMenuItemSpriteExtra::create(
            sendSpr, this, menu_selector(DecoMindPopup::onSend)
        );
        m_buttonMenu->addChildAtPosition(sendBtn, Anchor::BottomLeft, ccp(361.f, 138.f));

        this->label("RESULTADO", "bigFont.fnt", .35f, ccp(272.f, 116.f), ccc3(0x6a, 0xd2, 0xf5));
        m_output = SimpleTextArea::create(" ", "chatFont.fnt", .5f, 178.f);
        m_output->setWrappingMode(WORD_WRAP);
        m_output->setMaxLines(8);
        m_output->setAnchorPoint({0.5f, 1.f});
        m_output->setPosition({361.f, 106.f});
        m_mainLayer->addChild(m_output, 10);

        this->selectMode(m_mode);
        this->refreshHistory();
        this->refreshUsageFromSaved();
        this->setOutput("Elige un modo, escribe lo que quieres y pulsa Enviar.");
        return true;
    }

    void selectMode(int i) {
        m_mode = i;
        for (size_t k = 0; k < m_modeSprites.size(); ++k) {
            m_modeSprites[k]->setColor(
                static_cast<int>(k) == i ? ccc3(255, 255, 255) : ccc3(90, 120, 175)
            );
        }
        m_modeLabel->setString(fmt::format("Modo activo: {}", MODES[i].name).c_str());
        m_desc->setText(MODES[i].desc);
    }

    void onMode(CCObject* sender) {
        this->selectMode(static_cast<CCNode*>(sender)->getTag());
    }

    void setOutput(std::string const& text) {
        m_output->setText(text.empty() ? std::string(" ") : text);
        m_output->setPosition({361.f, 106.f});
    }

    void updateUsage(int used, int limit) {
        if (limit < 1) limit = 1;
        float frac = std::min(1.f, std::max(0.f, static_cast<float>(used) / limit));
        m_barFill->changeWidth(std::max(0.01f, BAR_W * frac));
        m_usageLabel->setString(fmt::format("Limite diario   {} / {}", used, limit).c_str());
    }

    void refreshUsageFromSaved() {
        auto source = Mod::get()->getSettingValue<std::string>("mode");
        if (source == "local") {
            m_barFill->changeWidth(0.01f);
            m_usageLabel->setString("Origen local: sin limite");
            return;
        }
        int used = Mod::get()->getSavedValue<int>("used", 0);
        int limit = Mod::get()->getSavedValue<int>("limit", 20);
        this->updateUsage(used, limit);
    }

    void refreshHistory() {
        auto& h = history();
        for (int i = 0; i < 3; ++i) {
            std::string s = i < static_cast<int>(h.size()) ? h[i] : std::string();
            if (s.size() > 40) s = s.substr(0, 40);
            m_hist[i]->setString(s.c_str());
        }
    }

    void pushHistory(std::string const& prompt) {
        auto& h = history();
        h.insert(h.begin(), fmt::format("{}: {}", MODES[m_mode].name, prompt));
        if (h.size() > 3) h.resize(3);
        this->refreshHistory();
    }

    void handleReply(std::string const& body) {
        std::string msg = getField(body, "MSG");
        std::string objs = getField(body, "OBJ");
        std::string usedS = getField(body, "USED");
        std::string limitS = getField(body, "LIMIT");

        if (!usedS.empty() && !limitS.empty()) {
            int used = std::atoi(usedS.c_str());
            int limit = std::atoi(limitS.c_str());
            Mod::get()->setSavedValue<int>("used", used);
            Mod::get()->setSavedValue<int>("limit", limit);
            this->updateUsage(used, limit);
        }

        std::string out = msg.empty() ? std::string("(el servidor no envio mensaje)") : msg;
        if (!objs.empty()) {
            m_lel->createObjectsFromString(objs, true, true);
            out += "\n\nObjetos colocados en el editor.";
        }
        this->setOutput(out);
    }

    void onSend(CCObject*) {
        if (m_busy) return;

        std::string prompt(m_input->getString().c_str());
        if (prompt.empty()) {
            this->setOutput("Escribe una peticion primero.");
            return;
        }

        auto source = Mod::get()->getSettingValue<std::string>("mode");
        if (source == "local") {
            int sections = static_cast<int>(Mod::get()->getSettingValue<int64_t>("sections"));
            m_lel->createObjectsFromString(makeLocalLevel(sections), true, true);
            this->pushHistory(prompt);
            this->setOutput(fmt::format(
                "Origen local (prueba): coloque {} secciones de layout. El modo elegido no cambia nada todavia.",
                sections
            ));
            return;
        }

        auto url = Mod::get()->getSettingValue<std::string>("server-url");
        auto req = web::WebRequest();
        req.bodyString(std::string(MODES[m_mode].id) + "\n" + prompt);
        req.timeout(std::chrono::seconds(60));

        m_busy = true;
        this->pushHistory(prompt);
        this->setOutput("Pensando...");

        m_task.spawn(
            "decomind-request",
            req.post(url),
            [this](web::WebResponse res) {
                m_busy = false;
                if (!res.ok()) {
                    this->setOutput(fmt::format(
                        "Error del servidor: {}. Revisa que el servidor este corriendo y la URL en los ajustes.",
                        res.code()
                    ));
                    return;
                }
                this->handleReply(res.string().unwrapOr(""));
            }
        );
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

// ---------------------------------------------------------------------------
// Boton "IA" en el editor
// ---------------------------------------------------------------------------
class $modify(DecoMindEditorUI, EditorUI) {
    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;

        auto spr = ButtonSprite::create("IA", "bigFont.fnt", "GJ_button_02.png", .8f);
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
