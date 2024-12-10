cmake_minimum_required(VERSION 3.16)

project(QodeAssist)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QtCreator REQUIRED COMPONENTS Core)
find_package(Qt6 COMPONENTS Core Gui Quick Widgets Network REQUIRED)

add_subdirectory(llmcore)
add_subdirectory(settings)
add_subdirectory(logger)
add_subdirectory(ChatView)

add_qtc_plugin(QodeAssist
  PLUGIN_DEPENDS
    QtCreator::Core
    QtCreator::LanguageClient
    QtCreator::TextEditor
    QtCreator::ProjectExplorer
  DEPENDS
    Qt::Core
    Qt::Gui
    Qt::Quick
    Qt::Widgets
    Qt::Network
    QtCreator::ExtensionSystem
    QtCreator::Utils
    QodeAssistChatViewplugin
  SOURCES
    .github/workflows/build_cmake.yml
    .github/workflows/README.md
    README.md
    qodeassist.cpp
    QodeAssistConstants.hpp
    QodeAssisttr.h
    LLMClientInterface.hpp LLMClientInterface.cpp
    templates/Templates.hpp
    templates/CodeLlamaFim.hpp
    templates/StarCoder2Fim.hpp
    templates/DeepSeekCoderFim.hpp
    templates/CustomFimTemplate.hpp
    templates/Qwen.hpp
    templates/Ollama.hpp
    templates/BasicChat.hpp
    templates/Llama3.hpp
    templates/ChatML.hpp
    templates/Alpaca.hpp
    templates/Llama2.hpp
    providers/Providers.hpp
    providers/OllamaProvider.hpp providers/OllamaProvider.cpp
    providers/LMStudioProvider.hpp providers/LMStudioProvider.cpp
    providers/OpenAICompatProvider.hpp providers/OpenAICompatProvider.cpp
    providers/OllamaMessage.hpp providers/OllamaMessage.cpp
    QodeAssist.qrc
    LSPCompletion.hpp
    LLMSuggestion.hpp LLMSuggestion.cpp
    QodeAssistClient.hpp QodeAssistClient.cpp
    DocumentContextReader.hpp DocumentContextReader.cpp
    utils/CounterTooltip.hpp utils/CounterTooltip.cpp
    core/ChangesManager.h core/ChangesManager.cpp
    chat/ChatOutputPane.h chat/ChatOutputPane.cpp
    chat/NavigationPanel.hpp chat/NavigationPanel.cpp
    ConfigurationManager.hpp ConfigurationManager.cpp
)
