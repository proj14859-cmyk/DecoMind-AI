#pragma once
#include "../LevelExecutor.hpp"
#include "../core/OperationSchema.hpp"
namespace decomind {
class EditorPerception {
public:
    static operations::Context capture(LevelEditorLayer* editor) {
        auto s = LevelExecutor::scanEditorContext(editor);
        operations::Context c;
        c.usedGroups = s.usedGroups; c.usedColors = s.usedColors;
        if (s.selectedCount) c.region = {double(s.selMinX - 45), double(s.selMaxX + 45), double(s.selMinY - 45), double(s.selMaxY + 45)};
        else c.region = {double(s.cameraX - 600), double(s.cameraX + 600), double(s.cameraY - 420), double(s.cameraY + 420)};
        return c;
    }
};
}
