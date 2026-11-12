#pragma once
#include "OperationSchema.hpp"
#include <matjson.hpp>
#include <stdexcept>
#include <cmath>
namespace decomind {
class PlanJson {
    static double number(matjson::Value const& v, char const* key, double fallback) {
        if (!v.contains(key)) return fallback;
        auto n = v[key].asDouble();
        if (!n) throw std::runtime_error("Propiedad numerica invalida.");
        return n.unwrap();
    }
    static int integer(matjson::Value const& v, char const* key, int fallback) {
        double n = number(v, key, fallback);
        if (!std::isfinite(n) || n < -1000000 || n > 1000000 || n != std::floor(n)) throw std::runtime_error("Entero invalido.");
        return static_cast<int>(n);
    }
public:
    static operations::Plan parse(std::string const& text) {
        if (text.size() > 512 * 1024) throw std::runtime_error("Propuesta demasiado grande.");
        auto parsed = matjson::parse(text);
        if (!parsed) throw std::runtime_error("La respuesta no es JSON valido.");
        auto j = parsed.unwrap();
        operations::Plan p;
        p.schemaVersion = integer(j, "schemaVersion", 0);
        p.requestId = j["requestId"].asString().unwrapOr("");
        auto ops = j["operations"].asArray();
        if (!ops || ops.unwrap().size() > 1000) throw std::runtime_error("Lista de operaciones invalida.");
        for (auto const& v : ops.unwrap()) {
            if (v["op"].asString().unwrapOr("") != "createObject") throw std::runtime_error("Solo createObject esta habilitado en esta alpha.");
            // No raw GD properties, triggers, deletion or updates may bypass validation.
            for (auto const& value : v) {
                auto key = value.getKey().value_or("");
                if (key != "op" && key != "tempId" && key != "objectId" && key != "position" && key != "rotation" && key != "scale" && key != "colorId" && key != "zLayer" && key != "zOrder" && key != "groups")
                    throw std::runtime_error("Propiedad de operacion no permitida.");
            }
            operations::CreateObject o;
            o.tempId = v["tempId"].asString().unwrapOr("");
            o.objectId = integer(v, "objectId", 0);
            o.x = number(v["position"], "x", NAN); o.y = number(v["position"], "y", NAN);
            o.rotation = number(v, "rotation", 0); o.scale = number(v, "scale", 1);
            o.colorId = integer(v, "colorId", 0); o.zLayer = integer(v, "zLayer", -1); o.zOrder = integer(v, "zOrder", 0);
            if (v.contains("groups")) {
                auto a = v["groups"].asArray();
                if (!a || a.unwrap().size() > 10) throw std::runtime_error("Grupos invalidos.");
                for (auto const& g : a.unwrap()) {
                    auto n = g.asInt();
                    if (!n || n.unwrap() < 1 || n.unwrap() > 9999) throw std::runtime_error("Grupo invalido.");
                    o.groups.push_back(static_cast<int>(n.unwrap()));
                }
            }
            p.objects.push_back(o);
        }
        return p;
    }
};
}
