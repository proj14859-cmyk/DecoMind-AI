#pragma once
#include "../core/OperationSchema.hpp"
namespace decomind {
class OfflineRuleProvider {
public:
    // Placement diagnostics must be explicitly requested. They are never a fallback
    // for natural-language instructions, even when automatic AI is unavailable.
    static operations::Plan diagnosticPlan(std::string const& mode, std::string const& prompt, operations::Context const& context) {
        operations::Plan plan;
        if (mode != "offline-rules" || prompt != "/prueba-nativa") return plan;
        plan.requestId = "explicit-native-smoke-test";
        for (int i = 0; i < 10; ++i) {
            operations::CreateObject object;
            object.tempId = "diagnostic-" + std::to_string(i);
            object.objectId = 15;
            object.x = context.region.minX + (context.region.maxX - context.region.minX) * (i + 1) / 11;
            object.y = i % 2 ? context.region.minY + 15 : context.region.maxY - 15;
            plan.objects.push_back(object);
        }
        return plan;
    }
};
}
