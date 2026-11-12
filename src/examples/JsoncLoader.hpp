#pragma once
#include <stdexcept>
#include <string>
namespace decomind {
class JsoncLoader {
public:
    // Preserve newlines and quoted URLs; comments are replaced with whitespace.
    static std::string stripComments(std::string text) {
        bool quoted = false, escape = false;
        for (size_t i = 0; i < text.size(); ++i) {
            char c = text[i];
            if (quoted) {
                if (escape) escape = false;
                else if (c == '\\') escape = true;
                else if (c == '"') quoted = false;
                continue;
            }
            if (c == '"') { quoted = true; continue; }
            if (c != '/' || i + 1 == text.size()) continue;
            if (text[i+1] == '/') {
                text[i++] = ' '; text[i] = ' ';
                while (i + 1 < text.size() && text[i+1] != '\n' && text[i+1] != '\r') text[++i] = ' ';
            } else if (text[i+1] == '*') {
                text[i++] = ' '; text[i] = ' ';
                bool closed = false;
                while (++i < text.size()) {
                    if (text[i] == '*' && i + 1 < text.size() && text[i+1] == '/') {
                        text[i++] = ' '; text[i] = ' '; closed = true; break;
                    }
                    if (text[i] != '\n' && text[i] != '\r') text[i] = ' ';
                }
                if (!closed) throw std::runtime_error("Comentario JSONC sin cerrar.");
            }
        }
        if (quoted) throw std::runtime_error("Cadena JSONC sin cerrar.");
        return text;
    }
};
}
