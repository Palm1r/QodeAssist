#include "FlowManager.hpp"

#include "DemoTasks.hpp"
#include "SimpleLLMTask.hpp"
#include "SimpleTextTask.hpp"

namespace QodeAssist::Flows::Tasks {

inline void registerTasks(FlowManager *flowManager)
{
    auto *taskRegistry = flowManager->taskRegistry();
    if (!taskRegistry) {
        return;
    }

    taskRegistry->registerTask<SimpleTextTask>("SimpleTextTask");
    taskRegistry->registerTask<SimpleLLMTask>("SimpleLLMTask");
    taskRegistry->registerTask<Task1>("Task1");
    taskRegistry->registerTask<Task2>("Task2");
}

} // namespace QodeAssist::Flows::Tasks
