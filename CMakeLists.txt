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
    PromptTemplateManager.hpp PromptTemplateManager.cpp
    templates/PromptTemplate.hpp
    templates/CodeLlamaFimTemplate.hpp
    templates/StarCoder2Template.hpp
    templates/DeepSeekCoderV2.hpp
    templates/CustomTemplate.hpp
    templates/DeepSeekCoderChatTemplate.hpp
    templates/CodeLlamaInstruct.hpp
    providers/LLMProvider.hpp
    providers/OllamaProvider.hpp providers/OllamaProvider.cpp
    providers/LMStudioProvider.hpp providers/LMStudioProvider.cpp
    providers/OpenAICompatProvider.hpp providers/OpenAICompatProvider.cpp
    LLMProvidersManager.hpp LLMProvidersManager.cpp
    QodeAssist.qrc
    LSPCompletion.hpp
    LLMSuggestion.hpp LLMSuggestion.cpp
    QodeAssistClient.hpp QodeAssistClient.cpp
    QodeAssistUtils.hpp
    DocumentContextReader.hpp DocumentContextReader.cpp
    QodeAssistData.hpp
    utils/CounterTooltip.hpp utils/CounterTooltip.cpp
    settings/GeneralSettings.hpp settings/GeneralSettings.cpp
    settings/ContextSettings.hpp settings/ContextSettings.cpp
    settings/CustomPromptSettings.hpp settings/CustomPromptSettings.cpp
    settings/PresetPromptsSettings.hpp settings/PresetPromptsSettings.cpp
    settings/SettingsUtils.hpp
    core/ChangesManager.h core/ChangesManager.cpp
    core/LLMRequestHandler.hpp core/LLMRequestHandler.cpp
    core/LLMRequestConfig.hpp
    chat/ChatWidget.h chat/ChatWidget.cpp
    chat/ChatOutputPane.h chat/ChatOutputPane.cpp
    chat/ChatClientInterface.hpp chat/ChatClientInterface.cpp
    chat/NavigationPanel.hpp chat/NavigationPanel.cpp
)

target_link_libraries(QodeAssist PUBLIC QodeAssistChatViewplugin)
