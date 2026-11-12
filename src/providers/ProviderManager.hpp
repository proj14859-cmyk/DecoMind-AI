#pragma once
#include <Geode/Geode.hpp>
#include "ProviderCredentials.hpp"
#include "../core/OperationSchema.hpp"
namespace decomind {
enum class ProviderState { Ready, Connecting, Running, RateLimited, Unavailable, AuthFailed, Canceled, Finished, Failed };
class ProviderManager {
public:
    static void migrate() {
        auto mod = geode::Mod::get();
        if (mod->getSavedValue<int>("settings-schema-version", 0) >= 1) return;
        auto old = mod->getSavedValue<std::string>("origin_mode", "");
        if (old.empty()) old = mod->getSettingValue<std::string>("mode");
        auto url = mod->getSavedValue<std::string>("server_url", "");
        if (url.empty()) url = mod->getSettingValue<std::string>("server-url");
        mod->setSavedValue("legacy-server-url", url);
        mod->setSettingValue<std::string>("ai-mode", old == "local" ? "offline-rules" : "auto");
        mod->setSavedValue("settings-schema-version", 1);
        (void)mod->saveData();
    }
    static std::string mode() { migrate(); return geode::Mod::get()->getSettingValue<std::string>("ai-mode"); }
    static std::string provider() { return geode::Mod::get()->getSettingValue<std::string>("ai-provider"); }
    static std::string endpoint() {
        return provider() == "groq" ? "https://api.groq.com/openai/v1/chat/completions" : "https://api.openai.com/v1/chat/completions";
    }
    static std::string model() { return geode::Mod::get()->getSettingValue<std::string>("ai-model"); }
    static std::string errorFor(int code) {
        if (code == 401 || code == 403) return "Credencial rechazada. Introduce o borra la clave en Ajustes.";
        if (code == 429) return "Proveedor ocupado o cuota agotada. No se cambio de proveedor.";
        if (code >= 500) return "Proveedor no disponible. Reintenta o elige Sin conexion.";
        return "Fallo HTTPS o timeout. No se modifico el nivel.";
    }
    static operations::Plan offline(operations::Context const& c, std::string const& prompt) {
        operations::Plan p; p.requestId = "offline-rules";
        // Small decorative frames around the work region; no gameplay or triggers.
        int object = prompt.find("espac") != std::string::npos ? 505 : 15;
        for (int i = 0; i < 10; ++i) {
            operations::CreateObject o;
            o.tempId = "rule-" + std::to_string(i); o.objectId = object;
            o.x = c.region.minX + (c.region.maxX - c.region.minX) * (i + 1) / 11;
            o.y = i % 2 ? c.region.minY + 15 : c.region.maxY - 15;
            o.scale = object == 505 ? .5 : 1;
            p.objects.push_back(o);
        }
        return p;
    }
};
}
