#include "../../src/validation/OperationValidator.hpp"
#include "../../src/core/PlanJson.hpp"
#include "../../src/editor/NativePlacementEngine.hpp"
#include "../../src/examples/ExampleLibrary.hpp"
#include "../../src/providers/OfflineRuleProvider.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace decomind;
void require(bool condition) { if (!condition) throw std::runtime_error("Check failed"); }
template<class F> void throws(F f) { bool caught = false; try { f(); } catch (...) { caught = true; } require(caught); }
int main(int argc, char** argv) {
    require(argc == 2);
    operations::Context c;
    for (auto const& mode : {"auto", "offline-rules"}) {
        for (auto const& prompt : {"deco moderna de una estructura", "espacial brillante en zona seleccionada", "Nine Circles glow con ruta de wave clara"}) {
            require(OfflineRuleProvider::diagnosticPlan(mode, prompt, c).objects.empty());
        }
    }
    require(OfflineRuleProvider::diagnosticPlan("auto", "/prueba-nativa", c).objects.empty());
    auto smoke = OfflineRuleProvider::diagnosticPlan("offline-rules", "/prueba-nativa", c);
    require(smoke.objects.size() == 10 && OperationValidator::validate(smoke, c).ok());
    auto make = [] {
        operations::Plan p; p.requestId = "test";
        operations::CreateObject o; o.tempId = "one"; o.objectId = 15; o.x = 50; o.y = 50;
        p.objects.push_back(o); return p;
    };
    for (int i = 0; i < 100; ++i) {
        auto p = make(); p.objects[0].x += i; p.objects[0].rotation = i; p.objects[0].scale = .1 + i * .03;
        require(OperationValidator::validate(p, c).ok());
    }
    auto reject = [&](auto change) { auto p = make(); change(p); require(!OperationValidator::validate(p, c).ok()); };
    reject([](auto& p) { p.objects[0].objectId = 4600; });
    reject([](auto& p) { p.objects[0].objectId = 1; });
    reject([](auto& p) { p.objects[0].x = std::numeric_limits<double>::quiet_NaN(); });
    reject([](auto& p) { p.objects[0].x = 1201; });
    reject([](auto& p) { p.objects[0].scale = 0; });
    reject([](auto& p) { p.objects[0].rotation = INFINITY; });
    reject([](auto& p) { p.objects[0].zLayer = 2; });
    reject([](auto& p) { p.objects[0].groups = {1,1}; });
    reject([](auto& p) { p.objects.push_back(p.objects[0]); });
    reject([](auto& p) { p.schemaVersion = 2; });
    c.usedGroups.insert(10); c.usedColors.insert(20);
    reject([](auto& p) { p.objects[0].groups = {10}; });
    reject([](auto& p) { p.objects[0].colorId = 20; });
    reject([](auto& p) { p.objects.resize(401, p.objects[0]); });
    auto original = std::string(R"({"schemaVersion":1,"requestId":"t","operations":[{"op":"createObject","tempId":"a","objectId":505,"position":{"x":50,"y":40}}]})");
    for (int i = 0; i < 100; ++i) {
        auto v = matjson::parse(original).unwrap();
        v["requestId"] = "parsed-" + std::to_string(i);
        v["operations"][0]["position"]["x"] = 50 + i;
        require(OperationValidator::validate(PlanJson::parse(v.dump()), c).ok());
    }
    auto p = PlanJson::parse(original);
    require(p.objects.size() == 1 && OperationValidator::validate(p, c).ok());
    require(NativePlacementEngine::serialize(p.objects[0]).find("1,505,2,50,3,40") == 0);
    throws([] { PlanJson::parse("{bad"); });
    throws([] { PlanJson::parse(R"({"schemaVersion":1,"requestId":"t","operations":[{"op":"deleteObject"}]})"); });
    throws([] { PlanJson::parse(R"({"schemaVersion":1,"requestId":"t","operations":[{"op":"createObject","rawGD":"1,9999"}]})"); });
    throws([] { PlanJson::parse(R"({"schemaVersion":1,"requestId":"t","operations":[{"op":"createObject","objectId":15.5}]})"); });
    throws([] { PlanJson::parse(R"({"schemaVersion":1,"requestId":"t","operations":[{"op":"createObject","objectId":15,"groups":[1.5]}]})"); });
    auto jsonc = JsoncLoader::stripComments("{ // comment\n\"url\":\"https://host/*literal*/\",/* multi\nline */\"v\":1}");
    auto j = matjson::parse(jsonc);
    require(j && j.unwrap()["url"].asString().unwrapOr("") == "https://host/*literal*/");
    throws([] { JsoncLoader::stripComments("{/* missing"); });
    throws([] { JsoncLoader::stripComments("{\"missing"); });
    auto examples = ExampleLibrary::retrieve(argv[1], "espacial brillante");
    require(examples.size() == 1 && examples[0].id == "decomind-space");
    require(ExampleLibrary::retrieve("/nonexistent", "space").empty());
    std::cout << "100 valid plans; invalid operations, schema, JSONC and references checked.\n";
}
