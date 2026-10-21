#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

// ---------------------------------------------------------------------------
// Generador LOCAL (modo "local"): no necesita red ni Termux.
// Formato de objeto de GD: "1,<id>,2,<x>,3,<y>;"
//   id 1 = bloque basico, id 8 = pico. Un bloque mide 30 unidades.
//   y = 15 es una casilla apoyada en el suelo.
// ---------------------------------------------------------------------------
static std::string makeLocalLevel(int sections) {
    std::string s;
    auto obj = [&](int id, int x, int y) {
        s += fmt::format("1,{},2,{},3,{};", id, x, y);
    };

    int x = 300; // margen inicial para que el jugador arranque tranquilo
    for (int i = 0; i < sections; ++i) {
        switch (i % 4) {
            case 0: // pico simple
                obj(8, x, 15);
                x += 240;
                break;
            case 1: // doble pico
                obj(8, x, 15);
                obj(8, x + 30, 15);
                x += 270;
                break;
            case 2: // bloque con pico encima (hay que saltar el bloque)
                obj(1, x, 15);
                obj(8, x + 90, 15);
                x += 270;
                break;
            case 3: // escalon de 3 bloques
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

class $modify(AIEditorUI, EditorUI) {
    struct Fields {
        EventListener<web::WebTask> m_listener;
    };

    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel)) return false;

        auto spr = ButtonSprite::create("IA");
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(AIEditorUI::onAI)
        );
        auto menu = CCMenu::create();
        menu->addChild(btn);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition({ win.width - 45.f, win.height - 70.f });
        menu->setID("ai-decorator-menu"_spr);
        this->addChild(menu, 100);
        return true;
    }

    void placeObjects(std::string const& str) {
        if (str.empty()) {
            Notification::create("La IA no devolvio objetos", NotificationIcon::Error)->show();
            return;
        }
        // Inyecta los objetos usando el formato nativo del juego.
        m_editorLayer->createObjectsFromString(str, true, true);
        Notification::create("Objetos colocados", NotificationIcon::Success)->show();
    }

    void onAI(CCObject*) {
        auto mode = Mod::get()->getSettingValue<std::string>("mode");

        if (mode == "local") {
            int sections = static_cast<int>(Mod::get()->getSettingValue<int64_t>("sections"));
            this->placeObjects(makeLocalLevel(sections));
            return;
        }

        // Modo "server": pide el nivel al servidor Python (Termux o PC).
        auto url = Mod::get()->getSettingValue<std::string>("server-url");
        auto prompt = Mod::get()->getSettingValue<std::string>("prompt");

        Notification::create("Pidiendo nivel a la IA...", NotificationIcon::Loading)->show();

        m_fields->m_listener.bind([this](web::WebTask::Event* e) {
            if (auto res = e->getValue()) {
                if (res->ok()) {
                    this->placeObjects(res->string().unwrapOr(""));
                } else {
                    Notification::create(
                        fmt::format("Error del servidor: {}", res->code()),
                        NotificationIcon::Error
                    )->show();
                }
            } else if (e->isCancelled()) {
                Notification::create("Peticion cancelada", NotificationIcon::Error)->show();
            }
        });

        auto req = web::WebRequest();
        req.bodyString(prompt);
        req.timeout(std::chrono::seconds(60));
        m_fields->m_listener.setFilter(req.post(url));
    }
};
