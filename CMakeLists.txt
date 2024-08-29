cmake_minimum_required(VERSION 3.16)

project(QodeAssist)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QtCreator REQUIRED COMPONENTS Core)
find_package(Qt6 COMPONENTS Widgets REQUIRED)

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
    templates/CodeLLamaTemplate.hpp
    templates/StarCoder2Template.hpp
    templates/CodeQwenChat.hpp
    providers/LLMProvider.hpp
    providers/OllamaProvider.hpp providers/OllamaProvider.cpp
    providers/LMStudioProvider.hpp providers/LMStudioProvider.cpp
    LLMProvidersManager.hpp LLMProvidersManager.cpp
    QodeAssistSettings.hpp QodeAssistSettings.cpp
    QodeAssist.qrc
    LSPCompletion.hpp
    LLMSuggestion.hpp LLMSuggestion.cpp
    QodeAssistHoverHandler.hpp QodeAssistHoverHandler.cpp
    QodeAssistClient.hpp QodeAssistClient.cpp
    QodeAssistUtils.hpp
    DocumentContextReader.hpp DocumentContextReader.cpp
)
