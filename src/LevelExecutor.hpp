#pragma once
// ===========================================================================
// DecoMind AI - Capa de Ejecucion de Acciones sobre el Nivel
//
// Se encarga de inspeccionar metadatos reales del nivel en LevelEditorLayer,
// gestionar copias de seguridad (GUARDAR / MIS NIVELES) y aplicar cadenas de
// objetos previamente validadas por LevelValidator.
// ===========================================================================
#include "LevelValidator.hpp"
#include "Modes.hpp"
#include <Geode/utils/file.hpp>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace decomind {

struct BackupInfo {
    std::string filename;
    std::string levelName;
    int objectCount = 0;
    std::string dateStr;
};

class LevelExecutor {
public:
    static std::string getLevelTitle(LevelEditorLayer* lel) {
        if (!lel || !lel->m_level) return "Sin titulo";
        std::string name = sanitizeForGD(std::string(lel->m_level->m_levelName));
        name = trimStr(name);
        return name.empty() ? "Sin titulo" : name;
    }

    static int getCurrentObjectCount(LevelEditorLayer* lel) {
        if (!lel || !lel->m_objects) return 0;
        return static_cast<int>(lel->m_objects->count());
    }

    static ValidationResult applyValidatedObjects(
        LevelEditorLayer* lel,
        std::string const& rawObjs,
        std::string const& modeId
    ) {
        ValidationResult vr = LevelValidator::validateObjects(rawObjs, modeId);
        if (!vr.allowed || vr.sanitizedObjects.empty() || !lel) {
            return vr;
        }
        lel->createObjectsFromString(vr.sanitizedObjects, true, true);
        return vr;
    }

    // Generador local de prueba (sin conexion a servidor) adaptado por modo
    static std::string generateLocalDemoObjects(std::string const& modeId, int sections) {
        if (modeId == "ideas" || modeId == "estructurar") return "";

        std::string s;
        auto obj = [&](int id, int x, int y) {
            s += fmt::format("1,{},2,{},3,{};", id, x, y);
        };

        int x = 300;
        for (int i = 0; i < sections; ++i) {
            if (modeId == "deco" || modeId == "efectos" || modeId == "transiciones" || modeId == "triggers") {
                // Objetos decorativos en altura sin colisionar en el suelo
                obj(1, x, 105 + (i % 3) * 30);
                obj(1, x + 30, 135);
                x += 240;
            } else if (modeId == "block_design") {
                // Estructura de pilares
                obj(1, x, 15);
                obj(1, x + 30, 15);
                obj(1, x, 45);
                obj(1, x + 30, 45);
                x += 210;
            } else {
                switch (i % 4) {
                    case 0:
                        obj(8, x, 15);
                        x += 240;
                        break;
                    case 1:
                        obj(8, x, 15);
                        obj(8, x + 30, 15);
                        x += 270;
                        break;
                    case 2:
                        obj(1, x, 15);
                        obj(8, x + 90, 15);
                        x += 270;
                        break;
                    case 3:
                        obj(1, x, 15);
                        obj(1, x + 30, 15);
                        obj(1, x + 30, 45);
                        obj(1, x + 60, 15);
                        obj(1, x + 60, 45);
                        obj(1, x + 60, 75);
                        x += 330;
                        break;
                }
            }
        }
        return s;
    }

    // Gestion de respaldos / puntos de guardado locales
    static std::filesystem::path backupsDir() {
        auto dir = Mod::get()->getSaveDir() / "backups";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir;
    }

    static bool saveLevelSnapshot(LevelEditorLayer* lel, std::string& outMsg) {
        if (!lel) {
            outMsg = "No hay un nivel activo en el editor.";
            return false;
        }
        std::string title = getLevelTitle(lel);
        int objCount = getCurrentObjectCount(lel);
        std::time_t now = std::time(nullptr);
        char timeBuf[32] = {0};
        if (auto tm = std::localtime(&now)) {
            std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", tm);
        } else {
            std::snprintf(timeBuf, sizeof(timeBuf), "%lld", static_cast<long long>(now));
        }

        std::string fname = fmt::format("snapshot_{}.txt", static_cast<long long>(now));
        std::string content = fmt::format(
            "LEVEL:{}\nOBJECTS:{}\nDATE:{}\n",
            title, objCount, timeBuf
        );

        auto res = utils::file::writeString(backupsDir() / fname, content);
        if (!res.isOk()) {
            outMsg = "No se pudo escribir el punto de guardado en disco.";
            return false;
        }
        outMsg = fmt::format("Punto de guardado creado para '{}' ({} objetos).", title, objCount);
        return true;
    }

    static std::vector<BackupInfo> listSnapshots() {
        std::vector<BackupInfo> list;
        std::error_code ec;
        auto dir = backupsDir();
        if (!std::filesystem::exists(dir, ec)) return list;

        for (auto const& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file()) continue;
            auto r = utils::file::readString(entry.path());
            if (!r.isOk()) continue;
            std::string body = r.unwrap();
            BackupInfo info;
            info.filename = entry.path().filename().string();
            info.levelName = extractLineValue(body, "LEVEL");
            info.objectCount = std::atoi(extractLineValue(body, "OBJECTS").c_str());
            info.dateStr = extractLineValue(body, "DATE");
            if (info.levelName.empty()) info.levelName = "Sin titulo";
            list.push_back(info);
        }
        std::sort(list.begin(), list.end(), [](BackupInfo const& a, BackupInfo const& b) {
            return a.filename > b.filename;
        });
        return list;
    }

private:
    static std::string extractLineValue(std::string const& body, std::string const& key) {
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
