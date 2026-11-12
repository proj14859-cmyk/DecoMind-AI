#pragma once
#include "LevelExecutor.hpp"
#include "editor/EditorPerception.hpp"
#include "core/PlanJson.hpp"
#include "providers/ProviderManager.hpp"
#include "providers/OfflineRuleProvider.hpp"
#include "examples/ExampleLibrary.hpp"
#include <asp/fs/std.hpp>
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
    bool m_testing = false;
    ProviderState m_state = ProviderState::Ready;
    async::TaskHolder<web::WebResponse> m_task;
    async::TaskHolder<web::WebResponse> m_probeTask;
    static std::string responseError(web::WebResponse& response) {
        auto base = ProviderManager::errorFor(response.code());
        auto body = response.string().unwrapOr("");
        if (body.size() > 64 * 1024) body.resize(64 * 1024);
        auto parsed = matjson::parse(body);
        if (!parsed) return base + " (HTTP " + std::to_string(response.code()) + ")";
        auto detail = parsed.unwrap()["error"]["message"].asString().unwrapOr("");
        detail = sanitizeForGD(detail);
        detail.erase(std::remove(detail.begin(), detail.end(), '\n'), detail.end());
        if (detail.size() > 240) detail.resize(240);
        if (detail.empty()) return base + " (HTTP " + std::to_string(response.code()) + ")";
        return base + " HTTP " + std::to_string(response.code()) + ": " + detail;
    }
public:
    using Callback = std::function<void(AIResponseData const&)>;
    ~AIClient() { cancel(); }
    bool isBusy() const { return m_busy || m_testing; }
    ProviderState state() const { return m_state; }
    void cancel() { m_task.cancel(); m_probeTask.cancel(); m_busy = false; m_testing = false; m_state = ProviderState::Canceled; }
    void checkConnection(std::function<void(bool)> callback) {
        // No unsolicited background network traffic or paid probes.
        if (callback) callback(m_state == ProviderState::Finished);
    }
    void testProvider(std::function<void(bool, std::string const&)> done) {
        if (isBusy()) { if (done) done(false, "Espera a que termine la solicitud actual."); return; }
        auto provider = ProviderManager::provider();
        auto key = ProviderCredentials::get(provider);
        if (key.empty()) { if (done) done(false, "Guarda primero una clave de sesion para el proveedor seleccionado."); return; }
        auto expectedModel = ProviderManager::model();
        auto req = web::WebRequest();
        req.header("Authorization", "Bearer " + key);
        req.followRedirects(false); req.timeout(std::chrono::seconds(15));
        m_testing = true; m_state = ProviderState::Connecting;
        m_probeTask.spawn("decomind-provider-test", req.get(ProviderManager::modelsEndpoint()),
            [this, done, expectedModel](web::WebResponse response) {
                m_testing = false;
                bool ok = false;
                std::string message;
                if (!response.ok()) message = responseError(response);
                else {
                    auto text = response.string().unwrapOr("");
                    if (text.size() > 2 * 1024 * 1024) message = "Lista de modelos demasiado grande.";
                    else {
                        auto parsed = matjson::parse(text);
                        if (parsed) {
                            auto models = parsed.unwrap()["data"].asArray();
                            if (models) for (auto const& model : models.unwrap())
                                if (model["id"].asString().unwrapOr("") == expectedModel) { ok = true; break; }
                        }
                        message = ok ? "Conexion y modelo disponibles: " + expectedModel + ". Prueba sin enviar datos del nivel."
                                     : "No se pudo confirmar el modelo " + expectedModel + ". Comprueba ai-model y permisos de tu cuenta.";
                    }
                }
                m_state = ok ? ProviderState::Finished : ProviderState::Failed;
                if (done) done(ok, message);
            });
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
        if (isBusy()) return;
        if (!editor || LevelEditorLayer::get() != editor) return;
        auto context = EditorPerception::capture(editor);
        std::vector<StyleReference> refs;
        auto resourcesPath = Mod::get()->getResourcesDir().toStdPath();
        if (resourcesPath) refs = ExampleLibrary::retrieve(resourcesPath.unwrap() / "examples" / "patterns.jsonc", prompt);
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
        auto body = matjson::makeObject({{"model", ProviderManager::model()}, {"messages", messages}, {"max_tokens", 4096}, {"response_format", matjson::makeObject({{"type", "json_object"}})}});
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
                r.message = responseError(response);
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
