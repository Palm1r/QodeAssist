cmake_minimum_required(VERSION 3.16)

project(QodeAssist)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QtCreator REQUIRED COMPONENTS Core)
find_package(Qt6 COMPONENTS Core Gui Widgets Network REQUIRED)

add_subdirectory(llmcore)
add_subdirectory(settings)
add_subdirectory(logger)
add_subdirectory(chatview)

add_qtc_plugin(QodeAssist
  PLUGIN_DEPENDS
    QtCreator::Core
    QtCreator::LanguageClient
    QtCreator::TextEditor
  DEPENDS
    Qt::Widgets
    QtCreator::ExtensionSystem
    QtCreator::Utils
    QtCreator::ProjectExplorer
  SOURCES
    .github/workflows/build_cmake.yml
    .github/workflows/README.md
    README.md
    qodeassist.cpp
    QodeAssistConstants.hpp
    QodeAssisttr.h
    LLMClientInterface.hpp LLMClientInterface.cpp
    templates/CodeLlamaFim.hpp
    templates/StarCoder2Fim.hpp
    templates/DeepSeekCoderFim.hpp
    templates/CustomFimTemplate.hpp
    templates/DeepSeekCoderChat.hpp
    templates/CodeLlamaChat.hpp
    templates/QwenChat.hpp
    providers/OllamaProvider.hpp providers/OllamaProvider.cpp
    providers/LMStudioProvider.hpp providers/LMStudioProvider.cpp
    providers/OpenAICompatProvider.hpp providers/OpenAICompatProvider.cpp
    QodeAssist.qrc
    LSPCompletion.hpp
    LLMSuggestion.hpp LLMSuggestion.cpp
    QodeAssistClient.hpp QodeAssistClient.cpp
    DocumentContextReader.hpp DocumentContextReader.cpp
    utils/CounterTooltip.hpp utils/CounterTooltip.cpp
    core/ChangesManager.h core/ChangesManager.cpp
    chat/ChatOutputPane.h chat/ChatOutputPane.cpp
    chat/NavigationPanel.hpp chat/NavigationPanel.cpp
)

target_link_libraries(QodeAssist PRIVATE QodeAssistChatViewplugin)
