cmake_minimum_required(VERSION 3.16)

project(QodeAssist)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(QtCreator REQUIRED COMPONENTS Core)
find_package(Qt6 COMPONENTS Core Gui Quick Widgets Network Test REQUIRED)
find_package(GTest)

# IDE_VERSION is defined by QtCreator package
string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" version_match ${IDE_VERSION})
set(QODEASSIST_QT_CREATOR_VERSION_MAJOR ${CMAKE_MATCH_1})
set(QODEASSIST_QT_CREATOR_VERSION_MINOR ${CMAKE_MATCH_2})
set(QODEASSIST_QT_CREATOR_VERSION_PATCH ${CMAKE_MATCH_3})

if(NOT version_match)
    message(FATAL_ERROR "Failed to parse Qt Creator version string: ${IDE_VERSION}")
endif()

message(STATUS "Qt Creator Version: ${QODEASSIST_QT_CREATOR_VERSION_MAJOR}.${QODEASSIST_QT_CREATOR_VERSION_MINOR}.${QODEASSIST_QT_CREATOR_VERSION_PATCH}")

add_definitions(
  -DQODEASSIST_QT_CREATOR_VERSION_MAJOR=${QODEASSIST_QT_CREATOR_VERSION_MAJOR}
  -DQODEASSIST_QT_CREATOR_VERSION_MINOR=${QODEASSIST_QT_CREATOR_VERSION_MINOR}
  -DQODEASSIST_QT_CREATOR_VERSION_PATCH=${QODEASSIST_QT_CREATOR_VERSION_PATCH}
)

add_subdirectory(llmcore)
add_subdirectory(settings)
add_subdirectory(logger)
add_subdirectory(ChatView)
add_subdirectory(context)
if(GTest_FOUND)
  add_subdirectory(test)
endif()

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
    templates/Ollama.hpp
    templates/Claude.hpp
    templates/OpenAI.hpp
    templates/MistralAI.hpp
    templates/StarCoder2Fim.hpp
    # templates/DeepSeekCoderFim.hpp
    # templates/CustomFimTemplate.hpp
    templates/Qwen.hpp
    templates/OpenAICompatible.hpp
    templates/Llama3.hpp
    templates/ChatML.hpp
    templates/Alpaca.hpp
    templates/Llama2.hpp
    templates/CodeLlamaQMLFim.hpp
    templates/GoogleAI.hpp
    templates/LlamaCppFim.hpp
    providers/Providers.hpp
    providers/OllamaProvider.hpp providers/OllamaProvider.cpp
    providers/ClaudeProvider.hpp providers/ClaudeProvider.cpp
    providers/OpenAIProvider.hpp providers/OpenAIProvider.cpp
    providers/MistralAIProvider.hpp providers/MistralAIProvider.cpp
    providers/LMStudioProvider.hpp providers/LMStudioProvider.cpp
    providers/OpenAICompatProvider.hpp providers/OpenAICompatProvider.cpp
    providers/OpenRouterAIProvider.hpp providers/OpenRouterAIProvider.cpp
    providers/GoogleAIProvider.hpp providers/GoogleAIProvider.cpp
    providers/LlamaCppProvider.hpp providers/LlamaCppProvider.cpp
    QodeAssist.qrc
    LSPCompletion.hpp
    LLMSuggestion.hpp LLMSuggestion.cpp
    QodeAssistClient.hpp QodeAssistClient.cpp
    chat/ChatOutputPane.h chat/ChatOutputPane.cpp
    chat/NavigationPanel.hpp chat/NavigationPanel.cpp
    ConfigurationManager.hpp ConfigurationManager.cpp
    CodeHandler.hpp CodeHandler.cpp
    UpdateStatusWidget.hpp UpdateStatusWidget.cpp
    widgets/CompletionProgressHandler.hpp widgets/CompletionProgressHandler.cpp
    widgets/ProgressWidget.hpp widgets/ProgressWidget.cpp
    widgets/EditorChatButton.hpp widgets/EditorChatButton.cpp
    widgets/EditorChatButtonHandler.hpp widgets/EditorChatButtonHandler.cpp
    QuickRefactorHandler.hpp QuickRefactorHandler.cpp
)

get_target_property(QtCreatorCorePath QtCreator::Core LOCATION)
find_program(QtCreatorExecutable
  NAMES
    qtcreator "Qt Creator"
  PATHS
    "${QtCreatorCorePath}/../../../bin"
    "${QtCreatorCorePath}/../../../MacOS"
  NO_DEFAULT_PATH
)
if (QtCreatorExecutable)
  add_custom_target(RunQtCreator
    COMMAND ${QtCreatorExecutable} -pluginpath $<TARGET_FILE_DIR:QodeAssist>
    DEPENDS QodeAssist
  )
  set_target_properties(RunQtCreator PROPERTIES FOLDER "qtc_runnable")
endif()
