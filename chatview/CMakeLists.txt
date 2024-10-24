qt_add_library(QodeAssistChatView STATIC)

qt_add_qml_module(QodeAssistChatView
    URI ChatView
    VERSION 1.0
    QML_FILES
    qml/RootItem.qml
    qml/ChatItem.qml
    qml/Badge.qml
    qml/dialog/CodeBlock.qml
    qml/dialog/TextBlock.qml
    qml/controls/SendButton.qml
    SOURCES
    ChatWidget.hpp ChatWidget.cpp
    ChatModel.hpp ChatModel.cpp
    ChatRootView.hpp ChatRootView.cpp
    ClientInterface.hpp ClientInterface.cpp
    MessagePart.hpp
    ChatUtils.h ChatUtils.cpp
    RESOURCES
    icons/clear-chat-icon.svg
    icons/bubble-icon.svg
    icons/bubble-pause-icon.svg
    icons/typing-bubble-1.svg
    icons/typing-bubble-2.svg
    icons/typing-bubble-3.svg
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
)

target_include_directories(QodeAssistChatView
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
