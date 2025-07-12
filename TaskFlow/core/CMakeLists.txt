qt_add_library(TaskFlowCore STATIC
    BaseTask.hpp BaseTask.cpp
    TaskConnection.hpp TaskConnection.cpp
    Flow.hpp Flow.cpp
    TaskPort.hpp TaskPort.cpp
    TaskRegistry.hpp TaskRegistry.cpp
    FlowManager.hpp FlowManager.cpp
    FlowRegistry.hpp FlowRegistry.cpp
)

target_link_libraries(TaskFlowCore
    PUBLIC
    Qt::Core
    Qt::Concurrent
    PRIVATE
    QodeAssistLogger
)

target_include_directories(TaskFlowCore
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
