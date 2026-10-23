#pragma once
// ===========================================================================
// DecoMind AI - Capa de Registro, Historial y Favoritos
//
// Gestiona el almacenamiento persistente de peticiones (historial.txt) y
// plantillas favoritas del usuario (favoritos.txt) dentro del directorio de
// guardado del mod en Geode.
// ===========================================================================
#include "Modes.hpp"
#include <Geode/utils/file.hpp>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace decomind {

struct HistoryEntry {
    std::string modeId;
    std::string prompt;
    std::time_t timestamp = 0;
    std::string summary;
};

struct FavoriteEntry {
    std::string modeId;
    std::string prompt;
};

class HistoryManager {
public:
    static std::vector<HistoryEntry>& entries() {
        static std::vector<HistoryEntry> list;
        return list;
    }

    static std::vector<FavoriteEntry>& favorites() {
        static std::vector<FavoriteEntry> favs;
        return favs;
    }

    static std::filesystem::path historyPath() {
        return Mod::get()->getSaveDir() / "historial.txt";
    }

    static std::filesystem::path favoritesPath() {
        return Mod::get()->getSaveDir() / "favoritos.txt";
    }

    static std::filesystem::path phrasesPath() {
        return Mod::get()->getSaveDir() / "frases.txt";
    }

    static void ensureLoaded() {
        static bool loaded = false;
        if (loaded) return;
        loaded = true;
        loadHistoryFromDisk();
        loadFavoritesFromDisk();
    }

    static void loadHistoryFromDisk() {
        entries().clear();
        auto res = utils::file::readString(historyPath());
        if (!res.isOk()) return;
        std::string text = res.unwrap();

        size_t pos = 0;
        while (pos < text.size()) {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos) end = text.size();
            std::string line = text.substr(pos, end - pos);
            pos = end + 1;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            // Formato compatible con versiones previas:
            // <modeId> \t <prompt> [\t <timestamp> [\t <summary>]]
            size_t t1 = line.find('\t');
            if (t1 == std::string::npos) continue;
            std::string modeId = line.substr(0, t1);
            std::string rest = line.substr(t1 + 1);

            std::string prompt = rest;
            std::time_t ts = 0;
            std::string summary;

            size_t t2 = rest.find('\t');
            if (t2 != std::string::npos) {
                prompt = rest.substr(0, t2);
                std::string afterPrompt = rest.substr(t2 + 1);
                size_t t3 = afterPrompt.find('\t');
                if (t3 != std::string::npos) {
                    ts = static_cast<std::time_t>(std::atoll(afterPrompt.substr(0, t3).c_str()));
                    summary = afterPrompt.substr(t3 + 1);
                } else {
                    ts = static_cast<std::time_t>(std::atoll(afterPrompt.c_str()));
                }
            }
            if (summary.empty()) {
                summary = buildShortSummary(modeId, prompt);
            }
            entries().push_back({modeId, prompt, ts, summary});
        }
    }

    static void saveHistoryToDisk() {
        auto& h = entries();
        if (h.size() > 100) {
            h.erase(h.begin(), h.begin() + static_cast<std::ptrdiff_t>(h.size() - 100));
        }
        std::string out;
        for (auto const& e : h) {
            out += e.modeId + "\t" + oneLineStr(e.prompt) + "\t" +
                    std::to_string(static_cast<long long>(e.timestamp)) + "\t" +
                    oneLineStr(e.summary) + "\n";
        }
        (void)utils::file::writeString(historyPath(), out);
    }

    static void addEntry(std::string const& modeId, std::string const& prompt) {
        std::time_t now = std::time(nullptr);
        std::string cleanPrompt = oneLineStr(sanitizeForGD(prompt));
        std::string summary = buildShortSummary(modeId, cleanPrompt);
        entries().push_back({modeId, cleanPrompt, now, summary});
        saveHistoryToDisk();
    }

    static void clearHistory() {
        entries().clear();
        saveHistoryToDisk();
    }

    static std::string buildShortSummary(std::string const& modeId, std::string const& prompt) {
        std::string p = trimStr(prompt);
        if (p.size() <= 28) return p;
        return p.substr(0, 26) + "..";
    }

    static std::string formatRelativeTime(std::time_t ts) {
        if (ts <= 0) return "Reciente";
        std::time_t now = std::time(nullptr);
        long long diff = static_cast<long long>(std::difftime(now, ts));
        if (diff < 60) return "Hace 1 min";
        long long mins = diff / 60;
        if (mins < 60) return fmt::format("Hace {} min", mins);
        long long hours = mins / 60;
        if (hours < 24) return fmt::format("Hace {} h", hours);
        long long days = hours / 24;
        return fmt::format("Hace {} d", days);
    }

    // Favoritos
    static void loadFavoritesFromDisk() {
        favorites().clear();
        auto res = utils::file::readString(favoritesPath());
        if (!res.isOk()) {
            // Inicializa favoritos utiles por defecto en la primera ejecucion
            favorites() = {
                {"deco", "Mejora la decoracion de la zona seleccionada"},
                {"transiciones", "Haz una transicion suave entre 2 zonas"},
                {"layout", "Crea un layout dinamico de 20 segundos"}
            };
            saveFavoritesToDisk();
            return;
        }
        std::string text = res.unwrap();
        size_t pos = 0;
        while (pos < text.size()) {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos) end = text.size();
            std::string line = text.substr(pos, end - pos);
            pos = end + 1;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            size_t tab = line.find('\t');
            if (tab == std::string::npos) continue;
            favorites().push_back({line.substr(0, tab), line.substr(tab + 1)});
        }
    }

    static void saveFavoritesToDisk() {
        std::string out;
        for (auto const& f : favorites()) {
            out += f.modeId + "\t" + oneLineStr(f.prompt) + "\n";
        }
        (void)utils::file::writeString(favoritesPath(), out);
    }

    static void addFavorite(std::string const& modeId, std::string const& prompt) {
        std::string clean = trimStr(oneLineStr(sanitizeForGD(prompt)));
        if (clean.empty()) return;
        for (auto const& f : favorites()) {
            if (f.modeId == modeId && f.prompt == clean) return;
        }
        favorites().push_back({modeId, clean});
        saveFavoritesToDisk();
    }

    static void removeFavorite(size_t idx) {
        if (idx < favorites().size()) {
            favorites().erase(favorites().begin() + static_cast<std::ptrdiff_t>(idx));
            saveFavoritesToDisk();
        }
    }

    // Frases del aliado en el editor (frases.txt)
    static std::vector<std::string> defaultPhrases() {
        return {
            "!Tu aliado en el editor de Geometry Dash!",
            "Una buena deco empieza con un buen layout.",
            "Menos es mas: deja respirar tu nivel.",
            "Prueba un pulso suave al ritmo de la musica.",
            "Contraste: fondo oscuro, detalles claros.",
            "Guarda una copia antes de cada cambio grande.",
            "Los triggers ordenados ahorran dolores de cabeza.",
            "Cada grupo libre es una oportunidad.",
            "Tip: pide sugerencias en Ideas y Sugerencias.",
            "La IA propone, tu decides."
        };
    }

    static std::vector<std::string> loadPhrases() {
        std::vector<std::string> out;
        auto path = phrasesPath();

        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            std::string def =
                "# Frases del aliado de DecoMind AI. Una por linea.\n"
                "# Las lineas que empiezan con # se ignoran.\n";
            for (auto const& p : defaultPhrases()) def += p + "\n";
            (void)utils::file::writeString(path, def);
        }

        auto res = utils::file::readString(path);
        if (res.isOk()) {
            std::string text = res.unwrap();
            size_t pos = 0;
            while (pos < text.size()) {
                size_t end = text.find('\n', pos);
                if (end == std::string::npos) end = text.size();
                std::string line = trimStr(text.substr(pos, end - pos));
                pos = end + 1;
                if (line.empty() || line[0] == '#') continue;
                out.push_back(sanitizeForGD(line));
            }
        }
        if (out.empty()) out = defaultPhrases();
        return out;
    }
};

} // namespace decomind
