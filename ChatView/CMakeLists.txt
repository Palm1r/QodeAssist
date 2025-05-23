qt_add_library(QodeAssistChatView STATIC)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_qml_module(QodeAssistChatView
    URI ChatView
    VERSION 1.0
    DEPENDENCIES QtQuick
    QML_FILES
    qml/RootItem.qml
    qml/ChatItem.qml
    qml/Badge.qml
    qml/dialog/CodeBlock.qml
    qml/dialog/TextBlock.qml
    qml/controls/QoAButton.qml
    qml/parts/TopBar.qml
    qml/parts/BottomBar.qml
    qml/parts/AttachedFilesPlace.qml

    RESOURCES
    icons/attach-file-light.svg
    icons/attach-file-dark.svg
    icons/close-dark.svg
    icons/close-light.svg
    icons/link-file-light.svg
    icons/link-file-dark.svg
    SOURCES
    ChatWidget.hpp ChatWidget.cpp
    ChatModel.hpp ChatModel.cpp
    ChatRootView.hpp ChatRootView.cpp
    ClientInterface.hpp ClientInterface.cpp
    MessagePart.hpp
    ChatUtils.h ChatUtils.cpp
    ChatSerializer.hpp ChatSerializer.cpp
)

target_link_libraries(QodeAssistChatView
    PUBLIC
    Qt::Widgets
    Qt::Quick
    Qt::QuickWidgets
    Qt::Network
    QtCreator::Core
    QtCreator::Utils
    LLMCore
    QodeAssistSettings
    Context
)

target_include_directories(QodeAssistChatView
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
