#pragma once
#include "../core/OperationSchema.hpp"
#include <sstream>
#include <locale>
namespace decomind {
class NativePlacementEngine {
public:
    static std::string serialize(operations::CreateObject const& o) {
        std::ostringstream s;
        s.imbue(std::locale::classic());
        s << "1," << o.objectId << ",2," << o.x << ",3," << o.y << ",6," << o.rotation
          << ",32," << o.scale << ",24," << o.zLayer << ",25," << o.zOrder;
        if (o.colorId) s << ",21," << o.colorId;
        if (!o.groups.empty()) {
            s << ",57,";
            for (size_t i = 0; i < o.groups.size(); ++i) { if (i) s << '.'; s << o.groups[i]; }
        }
        return s.str() + ";";
    }
};
}
