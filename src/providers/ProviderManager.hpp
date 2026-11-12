#pragma once
#include <Geode/Geode.hpp>
#include "ProviderCredentials.hpp"
#include "ProviderConfig.hpp"
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
    static std::string endpoint() { return ProviderConfig::baseUrl(provider()) + "/chat/completions"; }
    static std::string modelsEndpoint() { return ProviderConfig::baseUrl(provider()) + "/models"; }
    static std::string model() { return ProviderConfig::model(provider(), geode::Mod::get()->getSettingValue<std::string>("ai-model")); }
    static std::string errorFor(int code) {
        if (code == 401 || code == 403) return "Credencial rechazada. Introduce o borra la clave en Ajustes.";
        if (code == 429) return "Proveedor ocupado o cuota agotada. No se cambio de proveedor.";
        if (code >= 500) return "Proveedor no disponible. Reintenta o elige Sin conexion.";
        return "Fallo HTTPS o timeout. No se modifico el nivel.";
    }
};
}
