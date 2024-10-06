qt_add_library(QodeAssistChatView STATIC)

find_package(Qt6 COMPONENTS Core Widgets Quick QuickWidgets Network REQUIRED)

qt_add_qml_module(QodeAssistChatView
    URI ChatView
    VERSION 1.0
    QML_FILES
        qml/RootItem.qml
    SOURCES
        BaseChatWidget.hpp BaseChatWidget.cpp
        ChatModel.hpp ChatModel.cpp
)

target_link_libraries(QodeAssistChatView
    PRIVATE
    Qt::Widgets
    Qt::Quick
    Qt::QuickWidgets
    Qt::Network
)

target_include_directories(QodeAssistChatView
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
