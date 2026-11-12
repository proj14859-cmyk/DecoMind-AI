#pragma once
#include <Geode/Geode.hpp>
#include "../core/FileIO.hpp"
#include <chrono>
#include <filesystem>
namespace decomind {
class BackupManager {
public:
    static bool restoreCopy(std::filesystem::path const& path, std::string& message) {
        std::error_code ec;
        auto size = std::filesystem::file_size(path, ec);
        if (ec || size > 64 * 1024 * 1024) { message = "Respaldo ausente o demasiado grande."; return false; }
        auto content = FileIO::readString(path);
        if (!content) { message = "No se pudo leer el respaldo."; return false; }
        auto parsed = matjson::parse(content.unwrap());
        if (!parsed) { message = "Respaldo JSON invalido."; return false; }
        auto j = parsed.unwrap();
        auto data = j["levelData"].asString().unwrapOr("");
        if (j["schemaVersion"].asInt().unwrapOr(0) != 1 || data.empty()) { message = "Respaldo incompatible."; return false; }
        auto level = GameLevelManager::sharedState()->createNewLevel();
        if (!level) { message = "No se pudo crear la copia."; return false; }
        level->m_levelName = j["levelName"].asString().unwrapOr("DecoMind") + " (backup)";
        level->m_levelString = data;
        level->m_audioTrack = static_cast<int>(j["audioTrack"].asInt().unwrapOr(0));
        level->m_songID = static_cast<int>(j["songID"].asInt().unwrapOr(0));
        LocalLevelManager::sharedState()->save(false);
        message = "Copia recuperada en Mis niveles. El nivel abierto se conserva.";
        return true;
    }
    static bool save(LevelEditorLayer* editor, std::filesystem::path const& dir, std::string& message) {
        if (!editor || editor != LevelEditorLayer::get() || !editor->m_level) {
            message = "No hay un editor activo."; return false;
        }
        std::string data = editor->getLevelString();
        if (data.empty()) { message = "No se pudo serializar el nivel."; return false; }
        matjson::Value backup = matjson::makeObject({
            {"schemaVersion", 1}, {"levelName", std::string(editor->m_level->m_levelName)},
            {"levelData", data}, {"objectCount", static_cast<int>(editor->m_objects->count())},
            {"audioTrack", static_cast<int>(editor->m_level->m_audioTrack)},
            {"songID", static_cast<int>(editor->m_level->m_songID)}
        });
        auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        backup["timestamp"] = std::to_string(stamp);
        auto destination = dir / ("backup_" + std::to_string(stamp) + ".json");
        auto temporary = destination; temporary += ".tmp";
        if (!FileIO::writeString(temporary, backup.dump())) { message = "No se pudo escribir el respaldo."; return false; }
        auto check = FileIO::readString(temporary);
        auto parsed = check ? matjson::parse(check.unwrap()) : matjson::parse("");
        if (!parsed || parsed.unwrap()["levelData"].asString().unwrapOr("") != data) {
            std::error_code ec; std::filesystem::remove(temporary, ec);
            message = "Fallo la verificacion del respaldo."; return false;
        }
        std::error_code ec;
        std::filesystem::rename(temporary, destination, ec);
        if (ec) { std::filesystem::remove(temporary, ec); message = "No se pudo confirmar el respaldo."; return false; }
        message = "Respaldo completo guardado: " + destination.filename().string();
        return true;
    }
};
}
