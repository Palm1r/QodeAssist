qt_add_library(QodeAssistFlowEditor STATIC)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_qml_module(QodeAssistFlowEditor
    URI FlowEditor
    VERSION 1.0
    DEPENDENCIES QtQuick
    RESOURCES
    QML_FILES
    qml/FlowEditorView.qml
    SOURCES
    FlowEditor.hpp FlowEditor.cpp
)

target_link_libraries(QodeAssistFlowEditor
    PUBLIC
    Qt::Quick
    PRIVATE
    TaskManagement
)

target_include_directories(QodeAssistFlowEditor
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
