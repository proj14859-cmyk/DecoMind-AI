#pragma once
// ===========================================================================
// DecoMind AI - Capa de Validacion de Operaciones
//
// El modelo de IA o servidor puede proponer objetos/operaciones, pero el mod
// valida estrictamente todos los parametros (IDs de objeto, coordenadas X/Y,
// escalas, limites por lote y restricciones del modo) antes de tocar el nivel.
// ===========================================================================
#include "Modes.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace decomind {

struct ValidationResult {
    bool allowed = true;
    std::string sanitizedObjects;
    int validCount = 0;
    int rejectedCount = 0;
    std::string note;
};

class LevelValidator {
public:
    static constexpr int MAX_OBJECTS_PER_BATCH = 2000;
    static constexpr int MIN_OBJ_ID = 1;
    static constexpr int MAX_OBJ_ID = 4600;
    static constexpr float MIN_X = -240.f;
    static constexpr float MAX_X = 240000.f;
    static constexpr float MIN_Y = -240.f;
    static constexpr float MAX_Y = 6000.f;

    static ValidationResult validateObjects(std::string const& rawObjs, std::string const& modeId) {
        ValidationResult res;
        if (rawObjs.empty()) {
            return res;
        }

        // El modo "ideas" tiene prohibido modificar el nivel.
        if (modeId == "ideas") {
            res.allowed = false;
            res.note = "El modo Ideas y Sugerencias es solo consultivo y no modifica objetos del nivel.";
            return res;
        }

        size_t pos = 0;
        while (pos < rawObjs.size()) {
            size_t end = rawObjs.find(';', pos);
            if (end == std::string::npos) end = rawObjs.size();
            std::string entry = trimStr(rawObjs.substr(pos, end - pos));
            pos = end + 1;
            if (entry.empty()) continue;

            if (res.validCount >= MAX_OBJECTS_PER_BATCH) {
                res.rejectedCount++;
                continue;
            }

            if (isValidSingleObject(entry)) {
                res.sanitizedObjects += entry + ";";
                res.validCount++;
            } else {
                res.rejectedCount++;
            }
        }

        if (res.validCount == 0 && res.rejectedCount > 0) {
            res.allowed = false;
            res.note = fmt::format("Se descartaron {} objetos por parametros fuera de rango.", res.rejectedCount);
        } else if (res.rejectedCount > 0) {
            res.note = fmt::format("Validados {} objetos ({} omitidos por seguridad).", res.validCount, res.rejectedCount);
        }
        return res;
    }

private:
    static bool isValidSingleObject(std::string const& objStr) {
        // Formato de objeto GD: k1,v1,k2,v2,...
        // Clave 1 = ID de objeto, Clave 2 = X, Clave 3 = Y, Clave 32 = Escala
        std::vector<std::string> tokens;
        size_t p = 0;
        while (p < objStr.size()) {
            size_t c = objStr.find(',', p);
            if (c == std::string::npos) c = objStr.size();
            tokens.push_back(trimStr(objStr.substr(p, c - p)));
            p = c + 1;
        }

        if (tokens.size() < 6 || (tokens.size() % 2) != 0) {
            return false;
        }

        bool hasId = false;
        bool hasX = false;
        bool hasY = false;

        for (size_t i = 0; i + 1 < tokens.size(); i += 2) {
            int key = std::atoi(tokens[i].c_str());
            std::string const& val = tokens[i + 1];
            if (val.empty()) return false;

            if (key == 1) {
                int id = std::atoi(val.c_str());
                if (id < MIN_OBJ_ID || id > MAX_OBJ_ID) return false;
                hasId = true;
            } else if (key == 2) {
                float x = static_cast<float>(std::atof(val.c_str()));
                if (!std::isfinite(x) || x < MIN_X || x > MAX_X) return false;
                hasX = true;
            } else if (key == 3) {
                float y = static_cast<float>(std::atof(val.c_str()));
                if (!std::isfinite(y) || y < MIN_Y || y > MAX_Y) return false;
                hasY = true;
            } else if (key == 32) {
                float sc = static_cast<float>(std::atof(val.c_str()));
                if (!std::isfinite(sc) || sc < 0.05f || sc > 20.f) return false;
            }
        }
        return hasId && hasX && hasY;
    }
};

} // namespace decomind
