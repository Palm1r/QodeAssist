qt_add_library(TaskFlowEditor STATIC)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_qml_module(TaskFlowEditor
    URI TaskFlow.Editor
    VERSION 1.0
    DEPENDENCIES QtQuick
    RESOURCES
    QML_FILES
    qml/FlowEditorView.qml
    qml/Flow.qml
    qml/Task.qml
    qml/TaskPort.qml
    qml/TaskParameter.qml
    qml/TaskConnection.qml
    SOURCES
    FlowEditor.hpp FlowEditor.cpp
    FlowsModel.hpp FlowsModel.cpp
    TaskItem.hpp TaskItem.cpp
    FlowItem.hpp FlowItem.cpp
    TaskModel.hpp TaskModel.cpp
    TaskPortItem.hpp TaskPortItem.cpp
    TaskPortModel.hpp TaskPortModel.cpp
    TaskConnectionsModel.hpp TaskConnectionsModel.cpp
    TaskConnectionItem.hpp TaskConnectionItem.cpp
    GridBackground.hpp GridBackground.cpp
)

target_link_libraries(TaskFlowEditor
    PUBLIC
    Qt::Quick
    PRIVATE
    TaskFlowCore
)

target_include_directories(TaskFlowEditor
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)
