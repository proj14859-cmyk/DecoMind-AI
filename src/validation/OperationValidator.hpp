#pragma once
#include "../core/OperationSchema.hpp"
#include "../catalog/GDObjectCatalog.hpp"
#include <cmath>
#include <algorithm>
namespace decomind {
class OperationValidator {
public:
    static operations::Report validate(operations::Plan const& p, operations::Context const& c) {
        operations::Report r;
        auto fail = [&](std::string const& s) { if (r.errors.size() < 20) r.errors.push_back(s); };
        if (p.schemaVersion != 1 || p.requestId.empty() || p.requestId.size() > 128) fail("Version o requestId invalido.");
        if (p.objects.empty() || p.objects.size() > std::min<size_t>(c.maxObjects, 1000)) fail("Presupuesto de objetos excedido o propuesta vacia.");
        std::set<std::string> ids;
        std::set<int> groups, colors;
        for (auto const& o : p.objects) {
            auto entry = GDObjectCatalog::find(o.objectId);
            if (!entry || (entry->gameplay && !c.allowGameplay)) fail("ID desconocido o gameplay no autorizado.");
            if (o.tempId.empty() || o.tempId.size() > 128 || !ids.insert(o.tempId).second) fail("tempId vacio o duplicado.");
            if (!std::isfinite(o.x) || !std::isfinite(o.y) || !c.region.contains(o.x, o.y)) fail("Objeto fuera de la region de trabajo.");
            if (!std::isfinite(o.rotation) || std::abs(o.rotation) > 360 || !std::isfinite(o.scale) || o.scale < .1 || o.scale > 4) fail("Escala o rotacion invalida.");
            constexpr int layers[] = {-5, -3, -1, 1, 3, 5, 7, 9};
            if (std::find(std::begin(layers), std::end(layers), o.zLayer) == std::end(layers) || o.zOrder < -100 || o.zOrder > 100) fail("Capa u orden Z invalido.");
            if (o.colorId < 0 || o.colorId > 999 || (o.colorId && c.usedColors.count(o.colorId))) fail("Canal de color ocupado o invalido.");
            if (o.colorId) colors.insert(o.colorId);
            std::set<int> own;
            if (o.groups.size() > 10) fail("Demasiados grupos en un objeto.");
            for (int g : o.groups) {
                if (g < 1 || g > 9999 || c.usedGroups.count(g) || !own.insert(g).second) fail("Grupo ocupado, duplicado o invalido.");
                groups.insert(g);
            }
        }
        if (groups.size() > c.maxGroups || colors.size() > c.maxColors) fail("Presupuesto de grupos o colores excedido.");
        return r;
    }
};
}
