#pragma once
// ===========================================================================
// DecoMind AI v0.3.4-alpha - Capa de Validacion de Operaciones y Propiedades GD
//
// El modelo de IA o servidor puede proponer objetos/operaciones, pero el mod
// valida estrictamente todos los parametros: IDs oficiales de GD 2.2,
// coordenadas X/Y, rotacion, escala, canales de color (21/22/23), capas Z
// (20/24/25), grupos de objetos simples o multiples separados por punto (57),
// y propiedades completas de triggers (Move, Alpha, Pulse, Color, Rotate,
// Scale, Toggle, Spawn, Follow, Stop) antes de tocar el nivel.
// ===========================================================================
#include "Modes.hpp"
#include <algorithm>
#include <cctype>
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

        bool gameplayAllowed = modeAllowsGameplayObjects(modeId);

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

            if (isValidSingleObject(entry, gameplayAllowed)) {
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

    // Solo estos modos pueden colocar objetos con hitbox peligrosa o portales de gameplay.
    static bool modeAllowsGameplayObjects(std::string const& modeId) {
        return modeId == "layout" || modeId == "layout_deco" || modeId == "block_design";
    }

    // Picos, portales, velocidades, pads y orbes con hitbox de gameplay.
    // Segunda barrera: el servidor ya los filtra, pero el mod valida siempre en cliente.
    static bool isGameplayObjectId(int id) {
        static constexpr int IDS[] = {
            8, 9, 39, 103, 392,                        // picos
            10, 11, 12, 13, 45, 46, 47, 99, 101, 111,  // portales
            286, 287, 660, 745, 1331, 1933,
            200, 201, 202, 203, 1334,                  // velocidad
            35, 67, 140, 1332,                         // pads
            36, 84, 141, 1022, 1330, 1333, 1704, 1751  // orbes
        };
        for (int v : IDS) if (v == id) return true;
        return false;
    }

    // Valida una cadena de grupos de GD (clave 57), que puede ser un entero "12"
    // o hasta 10 grupos separados por punto "12.13.14".
    static bool isValidGroupDotString(std::string const& val) {
        if (val.empty()) return false;
        int groupCount = 0;
        size_t pos = 0;
        while (pos < val.size()) {
            size_t dot = val.find('.', pos);
            if (dot == std::string::npos) dot = val.size();
            std::string part = val.substr(pos, dot - pos);
            if (part.empty() || part.size() > 4) return false;
            for (char c : part) {
                if (!std::isdigit(static_cast<unsigned char>(c))) return false;
            }
            int gid = std::atoi(part.c_str());
            if (gid < 1 || gid > 9999) return false;
            ++groupCount;
            if (groupCount > 10) return false;
            pos = dot + 1;
        }
        return groupCount > 0;
    }

private:
    static bool isValidSingleObject(std::string const& objStr, bool gameplayAllowed) {
        // Formato oficial de objeto GD (LevelString): k1,v1,k2,v2,...
        // Clave 1 = ID de objeto, Clave 2 = X, Clave 3 = Y, Clave 6 = Rotacion,
        // Clave 21/22 = Color 1/2, Clave 24/25 = Z-Layer/Z-Order, Clave 32 = Escala,
        // Clave 57 = Grupos (ej. "10" o "10.11.12"), Claves de triggers (7..87).
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
            if (key <= 0 || val.empty()) return false;

            if (key == 1) {
                int id = std::atoi(val.c_str());
                if (id < MIN_OBJ_ID || id > MAX_OBJ_ID) return false;
                if (!gameplayAllowed && isGameplayObjectId(id)) return false;
                hasId = true;
            } else if (key == 2) {
                float x = static_cast<float>(std::atof(val.c_str()));
                if (!std::isfinite(x) || x < MIN_X || x > MAX_X) return false;
                hasX = true;
            } else if (key == 3) {
                float y = static_cast<float>(std::atof(val.c_str()));
                if (!std::isfinite(y) || y < MIN_Y || y > MAX_Y) return false;
                hasY = true;
            } else if (key == 4 || key == 5 || key == 11 || key == 17 || key == 36 ||
                       key == 41 || key == 42 || key == 56 || key == 62 || key == 64 ||
                       key == 67 || key == 70 || key == 87 || key == 100) {
                // Propiedades booleanas (0 o 1): FlipX/Y, Touch/Spawn/Multi-Trigger, Blending, Don't Fade/Enter, Lock
                int b = std::atoi(val.c_str());
                if (b != 0 && b != 1) return false;
            } else if (key == 6) {
                float rot = static_cast<float>(std::atof(val.c_str()));
                if (!std::isfinite(rot) || rot < -3600.f || rot > 3600.f) return false;
            } else if (key == 7 || key == 8 || key == 9) {
                int rgb = std::atoi(val.c_str());
                if (rgb < 0 || rgb > 255) return false;
            } else if (key == 10 || key == 45 || key == 46 || key == 47) {
                // Duracion / Fade In / Hold / Fade Out (segundos)
                float dur = static_cast<float>(std::atof(val.c_str()));
                if (!std::isfinite(dur) || dur < 0.f || dur > 120.f) return false;
            } else if (key == 20 || key == 61) {
                int el = std::atoi(val.c_str());
                if (el < 0 || el > 9999) return false;
            } else if (key == 21 || key == 22 || key == 23 || key == 50) {
                // Canales de color (1..1105)
                int col = std::atoi(val.c_str());
                if (col < 0 || col > 1105) return false;
            } else if (key == 24) {
                int zl = std::atoi(val.c_str());
                if (zl < -10 || zl > 15) return false;
            } else if (key == 25) {
                int zo = std::atoi(val.c_str());
                if (zo < -500 || zo > 500) return false;
            } else if (key == 28 || key == 29) {
                // Move X / Move Y (en unidades del editor)
                float mv = static_cast<float>(std::atof(val.c_str()));
                if (!std::isfinite(mv) || mv < -15000.f || mv > 15000.f) return false;
            } else if (key == 30) {
                // Easing (0..18)
                int eas = std::atoi(val.c_str());
                if (eas < 0 || eas > 18) return false;
            } else if (key == 32 || key == 150 || key == 151) {
                // Escala general / Scale X / Scale Y
                float sc = static_cast<float>(std::atof(val.c_str()));
                if (!std::isfinite(sc) || sc < 0.05f || sc > 20.f) return false;
            } else if (key == 35) {
                // Opacidad (0.0 .. 1.0)
                float op = static_cast<float>(std::atof(val.c_str()));
                if (!std::isfinite(op) || op < 0.f || op > 1.f) return false;
            } else if (key == 51 || key == 71) {
                // Target Group ID (51) / Center Group ID (71)
                int gid = std::atoi(val.c_str());
                if (gid < 0 || gid > 9999) return false;
            } else if (key == 52 || key == 48) {
                // Target Type (0=Channel/Group) / Pulse Mode (0=Color, 1=HSV)
                int md = std::atoi(val.c_str());
                if (md < 0 || md > 5) return false;
            } else if (key == 57) {
                // Group IDs asignados al objeto ("10" o "10.11.12")
                if (!isValidGroupDotString(val)) return false;
            } else if (key == 68 || key == 69) {
                // Rotate Degrees / Times 360
                float deg = static_cast<float>(std::atof(val.c_str()));
                if (!std::isfinite(deg) || deg < -36000.f || deg > 36000.f) return false;
            } else if (key == 85) {
                // Easing Rate
                float er = static_cast<float>(std::atof(val.c_str()));
                if (!std::isfinite(er) || er < 0.1f || er > 10.f) return false;
            }
        }
        return hasId && hasX && hasY;
    }
};

} // namespace decomind
