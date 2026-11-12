#pragma once
#include <array>
#include <string_view>
namespace decomind {
// Conservative bootstrap catalog. These are documented IDs, NOT runtime-verified.
// Add fixtures and device evidence before expanding this allowlist.
struct CatalogEntry { int id; std::string_view name; bool gameplay; std::string_view status; };
class GDObjectCatalog {
public:
    static constexpr std::array<CatalogEntry, 7> entries{{
        {1, "block", true, "documented"},
        {15, "decor-tall-rod", false, "documented"},
        {16, "decor-medium-rod", false, "documented"},
        {17, "decor-short-rod", false, "documented"},
        {503, "straight-medium-glow", false, "documented"},
        {504, "outer-corner-medium-glow", false, "documented"},
        {505, "inner-corner-medium-glow", false, "documented"}
    }};
    static CatalogEntry const* find(int id) {
        for (auto const& entry : entries) if (entry.id == id) return &entry;
        return nullptr;
    }
};
}
