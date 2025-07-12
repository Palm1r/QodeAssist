add_subdirectory(core)
add_subdirectory(Editor)
# add_subdirectory(serialization)
# add_subdirectory(tasks)

qt_add_library(TaskFlow STATIC)

target_link_libraries(TaskFlow
    PUBLIC
    TaskFlowCore
    TaskFlowEditorplugin
    # TaskFlowSerialization
    # TaskFlowTasks
)
