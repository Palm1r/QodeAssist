qt_add_library(QodeAssistUIControls STATIC)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_qml_module(QodeAssistUIControls
    URI UIControls
    VERSION 1.0
    DEPENDENCIES QtQuick
    QML_FILES
    qml/Badge.qml
    qml/QoAButton.qml
    qml/QoATextSlider.qml
    qml/QoAComboBox.qml
    qml/FadeListItemAnimation.qml
    
    RESOURCES
    icons/dropdown-arrow-light.svg
    icons/dropdown-arrow-dark.svg
    QML_FILES qml/QoASeparator.qml
)

target_link_libraries(QodeAssistUIControls
    PRIVATE
    Qt6::Core
    Qt6::Quick
)

target_include_directories(QodeAssistUIControls
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
