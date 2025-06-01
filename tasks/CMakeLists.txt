qt_add_library(TaskManagement STATIC
    BaseTask.hpp BaseTask.cpp
    TaskConnection.hpp
    Flow.hpp Flow.cpp
    TaskPort.hpp TaskPort.cpp
    TaskConnection.cpp
    TaskRegistry.hpp TaskRegistry.cpp
    RegisterTasksList.hpp
    DemoTasks.hpp DemoTasks.cpp
    SimpleLLMTask.hpp SimpleLLMTask.cpp
    FlowManager.hpp FlowManager.cpp
)

target_link_libraries(TaskManagement
    PUBLIC
    Qt::Core
    PRIVATE
    QodeAssistLogger
)

target_include_directories(TaskManagement
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
    ${CMAKE_SOURCE_DIR}
)
