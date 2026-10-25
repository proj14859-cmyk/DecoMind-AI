#pragma once
// ===========================================================================
// DecoMind AI - Capa de Comunicacion con el Servidor / Proveedor de IA
//
// Desacopla la interfaz visual de la red y del proveedor de IA.
// Construye las peticiones adjuntando internamente el preprompt del modo
// (internalPrompt) sin exponerlo en la UI, gestiona estados de carga,
// cancelacion y errores honestos cuando el servidor o proveedor no esta
// disponible o pendiente de configuracion.
// ===========================================================================
#include "LevelExecutor.hpp"
#include "Modes.hpp"
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>
#include <chrono>
#include <functional>
#include <string>

namespace decomind {

struct AIResponseData {
    bool ok = false;
    int httpCode = 0;
    std::string message;
    std::string rawObjects;
    int usedToday = -1;
    int dailyLimit = -1;
    bool isLocalDemo = false;
};

class AIClient {
public:
    using Callback = std::function<void(AIResponseData const&)>;

    bool isBusy() const {
        return m_busy;
    }

    void cancel() {
        if (m_busy) {
            m_task.cancel();
            m_busy = false;
        }
    }

    static std::string getOriginMode() {
        return Mod::get()->getSettingValue<std::string>("mode");
    }

    static void setOriginMode(std::string const& m) {
        Mod::get()->setSettingValue<std::string>("mode", m);
    }

    static std::string getServerUrl() {
        return Mod::get()->getSettingValue<std::string>("server-url");
    }

    static void setServerUrl(std::string const& url) {
        Mod::get()->setSettingValue<std::string>("server-url", url);
    }

    static int getLocalSections() {
        return static_cast<int>(Mod::get()->getSettingValue<int64_t>("sections"));
    }

    static void setLocalSections(int s) {
        int64_t clamped = std::max(1, std::min(40, s));
        Mod::get()->setSettingValue<int64_t>("sections", clamped);
    }

    void sendRequest(
        int modeIdx,
        std::string const& userPrompt,
        LevelEditorLayer* lel,
        Callback onComplete
    ) {
        if (m_busy) return;

        int safeIdx = (modeIdx >= 0 && modeIdx < 9) ? modeIdx : 2;
        auto const& mode = MODES[safeIdx];
        std::string origin = getOriginMode();

        // Modo local de demostracion / prueba sin red
        if (origin == "local") {
            AIResponseData res;
            res.ok = true;
            res.httpCode = 200;
            res.isLocalDemo = true;
            int secs = getLocalSections();
            res.rawObjects = LevelExecutor::generateLocalDemoObjects(mode.id, secs);
            if (std::string(mode.id) == "ideas") {
                res.message = fmt::format(
                    "[Modo Local - Prueba] Sugerencia para '{}': combina estructuras con contraste alto "
                    "y usa triggers de pulso suave en los cambios de ritmo. (Conecta el servidor en Ajustes "
                    "para respuestas de IA en tiempo real).",
                    sanitizeForGD(userPrompt)
                );
            } else {
                res.message = fmt::format(
                    "[Modo Local - Prueba] Se generaron {} secciones de prueba para el modo {}. "
                    "(Proveedor IA externo pendiente: cambia a 'Servidor' en Ajustes cuando tu servidor este activo).",
                    secs, mode.name
                );
            }
            if (onComplete) onComplete(res);
            return;
        }

        // Modo servidor HTTP (Python / proveedor IA)
        std::string url = trimStr(getServerUrl());
        if (url.empty()) {
            AIResponseData res;
            res.ok = false;
            res.httpCode = 0;
            res.message = "Configuracion pendiente: la URL del servidor esta vacia. Ve a Ajustes para configurarla.";
            if (onComplete) onComplete(res);
            return;
        }

        auto req = web::WebRequest();
        // Aqui se adjunta internalPrompt (preprompt interno del modo) sin mostrarlo en la interfaz
        std::string payload =
            std::string("MODE:") + mode.id + "\n" +
            "CONTEXT:" + oneLineStr(mode.internalPrompt) + "\n" +
            "LEVEL:" + oneLineStr(LevelExecutor::getLevelTitle(lel)) + "\n" +
            "PROMPT:" + oneLineStr(sanitizeForGD(userPrompt)) + "\n";

        req.bodyString(payload);
        req.timeout(std::chrono::seconds(45));

        m_busy = true;
        m_task.spawn(
            "decomind-ai-request",
            req.post(url),
            [this, onComplete](web::WebResponse webRes) {
                m_busy = false;
                AIResponseData out;
                out.httpCode = webRes.code();
                if (!webRes.ok()) {
                    out.ok = false;
                    out.message = fmt::format(
                        "No se pudo conectar con el servidor (codigo {}). Verifica que Server/server.py "
                        "este en ejecucion y que la URL en Ajustes sea correcta.",
                        webRes.code()
                    );
                    if (onComplete) onComplete(out);
                    return;
                }

                std::string body = webRes.string().unwrapOr("");
                out.ok = true;
                out.message = sanitizeForGD(extractField(body, "MSG"));
                out.rawObjects = extractField(body, "OBJ");

                std::string usedS = extractField(body, "USED");
                std::string limitS = extractField(body, "LIMIT");
                if (!usedS.empty()) out.usedToday = std::atoi(usedS.c_str());
                if (!limitS.empty()) out.dailyLimit = std::atoi(limitS.c_str());

                if (out.message.empty()) {
                    out.message = "Respuesta recibida del servidor (sin mensaje de texto).";
                }
                if (onComplete) onComplete(out);
            }
        );
    }

private:
    bool m_busy = false;
    async::TaskHolder<web::WebResponse> m_task;

    static std::string extractField(std::string const& body, std::string const& key) {
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
};

} // namespace decomind
