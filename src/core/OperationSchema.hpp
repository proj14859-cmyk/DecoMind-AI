#pragma once
#include <string>
#include <vector>
#include <set>

namespace decomind::operations {
struct Region {
    double minX = 0, maxX = 1200, minY = 0, maxY = 840;
    bool contains(double x, double y) const {
        return x >= minX && x <= maxX && y >= minY && y <= maxY;
    }
};
struct CreateObject {
    std::string tempId;
    int objectId = 0;
    double x = 0, y = 0, rotation = 0, scale = 1;
    int colorId = 0, zLayer = -1, zOrder = 0;
    std::vector<int> groups;
};
struct Plan {
    int schemaVersion = 1;
    std::string requestId;
    std::string summary;
    std::vector<CreateObject> objects;
    std::vector<std::string> references;
};
struct Context {
    Region region;
    std::set<int> usedGroups, usedColors;
    size_t maxObjects = 400, maxGroups = 30, maxColors = 12;
    bool allowGameplay = false;
};
struct Report {
    std::vector<std::string> errors;
    bool ok() const { return errors.empty(); }
};
} // namespace decomind::operations
