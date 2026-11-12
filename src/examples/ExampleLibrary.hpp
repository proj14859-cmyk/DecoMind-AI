#pragma once
#include "JsoncLoader.hpp"
#include <matjson.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <vector>
namespace decomind {
struct StyleReference { std::string id, source, pattern; int score; };
class ExampleLibrary {
public:
    static std::vector<StyleReference> retrieve(std::filesystem::path const& path, std::string prompt) {
        std::vector<StyleReference> result;
        std::error_code ec;
        auto size = std::filesystem::file_size(path, ec);
        if (ec || size > 128 * 1024) return result;
        std::ifstream file(path);
        std::string text((std::istreambuf_iterator<char>(file)), {});
        std::transform(prompt.begin(), prompt.end(), prompt.begin(), [](unsigned char c) { return std::tolower(c); });
        try {
            auto parsed = matjson::parse(JsoncLoader::stripComments(text));
            if (!parsed) return result;
            auto examples = parsed.unwrap()["examples"].asArray();
            if (!examples || examples.unwrap().size() > 100) return result;
            for (auto const& e : examples.unwrap()) {
                int score = 0;
                auto tags = e["tags"].asArray();
                if (!tags) continue;
                for (auto const& tag : tags.unwrap()) {
                    auto t = tag.asString().unwrapOr("");
                    if (!t.empty() && prompt.find(t) != std::string::npos) ++score;
                }
                if (score) result.push_back({e["id"].asString().unwrapOr(""), e["source"].asString().unwrapOr(""), e["pattern"].asString().unwrapOr(""), score});
            }
        } catch (...) { return {}; }
        std::stable_sort(result.begin(), result.end(), [](auto const& a, auto const& b) { return a.score > b.score; });
        if (result.size() > 5) result.resize(5);
        return result;
    }
};
}
