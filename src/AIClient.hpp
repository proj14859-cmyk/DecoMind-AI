#pragma once
#include "LevelExecutor.hpp"
#include "editor/EditorPerception.hpp"
#include "core/PlanJson.hpp"
#include "providers/ProviderManager.hpp"
#include "providers/OfflineRuleProvider.hpp"
#include "examples/ExampleLibrary.hpp"
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>
#include <chrono>
#include <functional>
namespace decomind {
struct AIResponseData {
    bool ok = false;
    int httpCode = 0;
    std::string message;
    operations::Plan plan;
    int usedToday = -1, dailyLimit = -1;
    bool isLocalDemo = false;
};
class AIClient {
    bool m_busy = false;
    ProviderState m_state = ProviderState::Ready;
    async::TaskHolder<web::WebResponse> m_task;
public:
    using Callback = std::function<void(AIResponseData const&)>;
    ~AIClient() { cancel(); }
    bool isBusy() const { return m_busy; }
    ProviderState state() const { return m_state; }
    void cancel() { m_task.cancel(); m_busy = false; m_state = ProviderState::Canceled; }
    void checkConnection(std::function<void(bool)> callback) {
        // No unsolicited background network traffic or paid probes.
        if (callback) callback(m_state == ProviderState::Finished);
    }
    // Compatibility adapter for the original UI; ai-mode is the sole source of truth.
    static std::string getOriginMode() { auto mode = ProviderManager::mode(); return (mode == "offline-rules" || mode == "auto") ? "local" : "server"; }
    static void setOriginMode(std::string const& m) {
        Mod::get()->setSettingValue<std::string>("ai-mode", m == "local" ? "offline-rules" : "api-personal");
        (void)Mod::get()->saveData();
    }
    static int getLocalSections() { return std::clamp(Mod::get()->getSavedValue<int>("local_sections", 8), 1, 40); }
    static void setLocalSections(int s) { Mod::get()->setSavedValue("local_sections", std::clamp(s, 1, 40)); }
    void uploadCatalog(std::function<void(bool, std::string const&)> callback) {
        if (callback) callback(true, "Catalogo inicial: 7 IDs documentados, 6 decorativos habilitados. Verificacion en juego pendiente. Consulta local, sin subida de datos.");
    }
    void sendRequest(int modeIdx, std::string const& prompt, LevelEditorLayer* editor, Callback done) {
        if (m_busy) return;
        if (!editor || LevelEditorLayer::get() != editor) return;
        auto context = EditorPerception::capture(editor);
        auto refs = ExampleLibrary::retrieve(Mod::get()->getResourcesDir() / "examples" / "patterns.jsonc", prompt);
        auto mode = ProviderManager::mode();
        if (mode == "offline-rules" || mode == "auto") {
            AIResponseData r;
            r.isLocalDemo = true;
            r.plan = OfflineRuleProvider::diagnosticPlan(mode, prompt, context);
            bool adviceOnly = std::string(MODES[std::clamp(modeIdx, 0, 8)].id) == "ideas";
            if (adviceOnly) {
                std::string advice;
                LevelExecutor::generateContextualLocalObjects(editor, "ideas", prompt, getLocalSections(), advice);
                r.ok = true;
                r.message = "Consejos por reglas, sin IA remota: " + advice;
            } else if (!r.plan.objects.empty()) {
                // Explicit placement diagnostic only. Never substitute this pattern for a user's request.
                r.ok = true;
                r.message = "Prueba nativa: patron fijo de 10 elementos. No interpreta instrucciones.";
            } else {
                m_state = ProviderState::Unavailable;
                r.message = mode == "auto"
                    ? "No hay un proveedor de IA automatico disponible. Elige API personal, configura proveedor/modelo y guarda tu clave de sesion. No se genero una decoracion de prueba en lugar de tu pedido."
                    : "El modo sin conexion aun no dispone de recetas que interpreten este pedido. Usa API personal para generar con IA. /prueba-nativa solo comprueba colocacion y rechazo con un patron fijo.";
            }
            if (done) done(r);
            return;
        }
        auto provider = ProviderManager::provider();
        auto key = ProviderCredentials::get(provider);
        if (mode == "free" || key.empty()) {
            AIResponseData r; r.message = mode == "free" ? "No hay un servicio gratuito sin cuenta autorizado. Elige Automatico, Sin conexion o API personal." : "Introduce la clave de sesion en Ajustes. Proveedor y modelo se eligen en los ajustes de Geode.";
            m_state = ProviderState::Unavailable; if (done) done(r); return;
        }
        int idx = std::clamp(modeIdx, 0, 8);
        std::string system = std::string("Eres DecoMind. Responde SOLO JSON sin Markdown. Esquema: {\"schemaVersion\":1,\"requestId\":\"unique\",\"summary\":\"Resumen en espanol\",\"operations\":[{\"op\":\"createObject\",\"tempId\":\"unique\",\"objectId\":15,\"position\":{\"x\":0,\"y\":0},\"scale\":1,\"zLayer\":-1}]}. ") +
            "Solo decoracion IDs 15,16,17,503,504,505. No triggers ni gameplay. Maximo 100 objetos. No grupos ni colores. Escala .1..4. " +
            "Region: x=" + std::to_string(context.region.minX) + ".." + std::to_string(context.region.maxX) + "; y=" + std::to_string(context.region.minY) + ".." + std::to_string(context.region.maxY) + ". " + MODES[idx].internalPrompt;
        if (std::string(MODES[idx].id) == "ideas") system += " Modo consultivo: operations debe estar vacio.";
        // Only the selected/visible geometry is shared, never the full level or history.
        auto scan = LevelExecutor::scanEditorContext(editor);
        system += "\nGeometria de referencia: " + LevelExecutor::serializeObjectsBrief(scan.selectedCount ? scan.selectedObjects : scan.viewportObjects, 40);
        for (auto const& ref : refs) system += "\nReferencia " + ref.id + " (" + ref.source + "): " + ref.pattern;
        auto messages = matjson::Value::array();
        messages.push(matjson::makeObject({{"role", "system"}, {"content", system}}));
        messages.push(matjson::makeObject({{"role", "user"}, {"content", prompt}}));
        auto body = matjson::makeObject({{"model", ProviderManager::model()}, {"messages", messages}, {"max_tokens", 4096}});
        auto req = web::WebRequest();
        req.header("Authorization", "Bearer " + key);
        req.header("Content-Type", "application/json");
        req.followRedirects(false);
        req.bodyJSON(body); req.timeout(std::chrono::seconds(45));
        m_busy = true; m_state = ProviderState::Running;
        m_task.spawn("decomind-direct-https", req.post(ProviderManager::endpoint()), [this, done, refs](web::WebResponse response) {
            m_busy = false;
            AIResponseData r; r.httpCode = response.code();
            if (!response.ok()) {
                m_state = r.httpCode == 429 ? ProviderState::RateLimited : (r.httpCode == 401 || r.httpCode == 403 ? ProviderState::AuthFailed : ProviderState::Failed);
                r.message = ProviderManager::errorFor(r.httpCode);
            } else {
                try {
                    auto text = response.string().unwrapOr("");
                    if (text.size() > 1024 * 1024) throw std::runtime_error("Respuesta demasiado grande.");
                    auto j = matjson::parse(text);
                    if (!j) throw std::runtime_error("Respuesta HTTPS invalida.");
                    auto content = j.unwrap()["choices"][0]["message"]["content"].asString().unwrapOr("");
                    r.plan = PlanJson::parse(content); r.ok = true;
                    for (auto const& ref : refs) r.plan.references.push_back(ref.id);
                    r.message = r.plan.summary.empty() ? "Plan recibido por HTTPS; pendiente de validacion y preview." : sanitizeForGD(r.plan.summary);
                    m_state = ProviderState::Finished;
                } catch (...) { m_state = ProviderState::Failed; r.message = "El proveedor devolvio una propuesta invalida o incompleta. No se aplico."; }
            }
            if (done) done(r);
        });
    }
};
}
