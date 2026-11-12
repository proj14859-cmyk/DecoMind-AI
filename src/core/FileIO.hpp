#pragma once
#include <Geode/utils/file.hpp>
#include <asp/fs/std.hpp>
#include <filesystem>
namespace decomind {
// Keep the existing std::filesystem-based backup/index code, converting at the
// Geode 6 boundary through asp's UTF-8-aware adapter (including Windows paths).
class FileIO {
public:
    static auto readString(std::filesystem::path const& path) {
        return geode::utils::file::readString(asp::fs::Path::fromStdPath(path));
    }
    static auto writeString(std::filesystem::path const& path, std::string const& data) {
        return geode::utils::file::writeString(asp::fs::Path::fromStdPath(path), data);
    }
};
}
