qt_add_library(QodeAssistChatView STATIC)

qt_add_qml_module(QodeAssistChatView
    URI ChatView
    VERSION 1.0
    QML_FILES
    qml/RootItem.qml
    SOURCES
    ChatWidget.hpp ChatWidget.cpp
    ChatModel.hpp ChatModel.cpp
    ChatRootView.hpp ChatRootView.cpp
    ClientInterface.hpp ClientInterface.cpp
)

target_link_libraries(QodeAssistChatView
    PUBLIC
    Qt::Widgets
    Qt::Quick
    Qt::QuickWidgets
    Qt::Network
    QtCreator::Core
    QtCreator::Utils
    PRIVATE
    LLMCore
    QodeAssistLogger
    QodeAssistSettings
)

target_include_directories(QodeAssistChatView
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
