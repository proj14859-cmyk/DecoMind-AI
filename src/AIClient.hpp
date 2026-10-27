#pragma once
// ===========================================================================
// DecoMind AI v0.3.4-alpha - Capa de Comunicacion con el Servidor / Proveedor IA
//
// Desacopla la interfaz visual de la red y del proveedor de IA.
// Construye las peticiones adjuntando internamente:
//   - El preprompt del modo (CONTEXT)
//   - El estado real del nivel, camara y objetos seleccionados (SELECTION / VIEWPORT)
//   - Los Group IDs ocupados y libres (USED_GROUPS / FREE_GROUPS)
//   - Los Color IDs ocupados y libres (USED_COLORS / FREE_COLORS)
// ===========================================================================
#include "LevelExecutor.hpp"
#include "ObjectCatalog.hpp"
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

        // Modo local contextual (funciona sin red leyendo seleccion, grupos y colores del nivel)
        if (origin == "local") {
            AIResponseData res;
            res.ok = true;
            res.httpCode = 200;
            res.isLocalDemo = true;
            int secs = getLocalSections();
            std::string summaryMsg;
            res.rawObjects = LevelExecutor::generateContextualLocalObjects(
                lel, mode.id, userPrompt, secs, summaryMsg
            );
            res.message = sanitizeForGD(summaryMsg);
            if (onComplete) onComplete(res);
            return;
        }

        // Modo servidor HTTP (Python / proveedor IA con lectura completa del nivel y seleccion)
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
        // Adjuntamos el preprompt del modo + el escaneo en vivo del nivel (seleccion, grupos y colores)
        std::string editorContextLines = LevelExecutor::buildProtocolContextLines(lel);
        std::string payload =
            std::string("MODE:") + mode.id + "\n" +
            "CONTEXT:" + oneLineStr(mode.internalPrompt) + "\n" +
            "LEVEL:" + oneLineStr(LevelExecutor::getLevelTitle(lel)) + "\n" +
            editorContextLines +
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

    // Sube el catalogo de objetos del juego al servidor (POST <servidor>/catalog).
    // Solo tiene sentido en modo "server". El callback recibe (ok, mensaje).
    void uploadCatalog(std::function<void(bool, std::string const&)> onDone) {
        if (getOriginMode() != "server") {
            if (onDone) onDone(false, "El catalogo se sube al servidor: cambia el Origen a 'server' en Ajustes.");
            return;
        }
        std::string url = trimStr(getServerUrl());
        if (url.empty()) {
            if (onDone) onDone(false, "La URL del servidor esta vacia (Ajustes).");
            return;
        }
        // http://IP:8765/generate  ->  http://IP:8765/catalog
        size_t g = url.rfind("/generate");
        url = (g != std::string::npos) ? url.substr(0, g) + "/catalog" : url + "/catalog";

        int count = 0;
        std::string body = ObjectCatalog::build(count);
        if (count == 0) {
            if (onDone) onDone(false, "No se pudo leer el catalogo de objetos del juego.");
            return;
        }

        auto req = web::WebRequest();
        req.bodyString(body);
        req.timeout(std::chrono::seconds(60));
        m_catalogTask.spawn(
            "decomind-catalog-upload",
            req.post(url),
            [onDone, count](web::WebResponse webRes) {
                if (!webRes.ok()) {
                    if (onDone) onDone(false, fmt::format(
                        "No se pudo subir el catalogo (codigo {}). Revisa que el servidor este activo.", webRes.code()));
                    return;
                }
                std::string reply = extractField(webRes.string().unwrapOr(""), "MSG");
                if (onDone) onDone(true, reply.empty() ? fmt::format("Catalogo enviado ({} objetos).", count) : reply);
            }
        );
    }

private:
    bool m_busy = false;
    async::TaskHolder<web::WebResponse> m_task;
    async::TaskHolder<web::WebResponse> m_catalogTask;

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
