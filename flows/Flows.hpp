#pragma once

#include "FlowManager.hpp"
#include "FlowRegistry.hpp"
#include "TestFileFlow.hpp"

namespace QodeAssist::Flows {

inline void registerFlows(FlowManager *flowManager)
{
    auto *flowRegistry = flowManager->flowRegistry();
    if (!flowRegistry) {
        return;
    }

    // Register flows
    flowRegistry->registerFlow("TestFileFlow", createTestFileFlow);
    auto flow = flowRegistry->createFlow("TestFileFlow", flowManager);
    flowManager->addFlow(flow);
}

} // namespace QodeAssist::Flows
