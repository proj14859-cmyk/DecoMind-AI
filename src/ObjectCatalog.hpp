#pragma once
// ===========================================================================
// DecoMind AI - Catalogo de objetos del juego
//
// Lee la tabla interna de GD (ObjectToolbox::m_allKeys: id de objeto -> nombre
// de su sprite) y la convierte en lineas "id|sprite". El servidor la usa para
// saber que IDs existen, cuales son decoracion y cuales afectan al gameplay.
// Se genera con el comando /catalogo del chat del mod.
// ===========================================================================
#include <Geode/Geode.hpp>
#include <string>

using namespace geode::prelude;

namespace decomind {

class ObjectCatalog {
public:
    // Devuelve el texto del catalogo (una linea "id|sprite" por objeto) y el total.
    static std::string build(int& outCount) {
        std::string out;
        outCount = 0;
        auto tb = ObjectToolbox::sharedState();
        if (!tb) return out;
        for (auto const& kv : tb->m_allKeys) {
            std::string frame = kv.second.c_str();
            if (frame.empty() || kv.first <= 0) continue;
            out += fmt::format("{}|{}\n", kv.first, frame);
            ++outCount;
        }
        return out;
    }
};

} // namespace decomind
