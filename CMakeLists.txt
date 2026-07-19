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
find_package(Qt6 COMPONENTS Core Gui Quick Widgets Network Svg LinguistTools REQUIRED)
set(QT_VERSION_MAJOR 6)

option(WITH_TESTS "Builds with tests" NO)

if(WITH_TESTS)
  find_package(Qt6 REQUIRED COMPONENTS Test)
endif()

qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES
    en cs zh_CN zh_TW da de fr hr ja pl ru sl sv uk
)

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

add_subdirectory(sources)

add_qtc_plugin(QodeAssist
  PLUGIN_DEPENDS
    QtCreator::Core
    QtCreator::LanguageClient
    QtCreator::TextEditor
    QtCreator::ProjectExplorer
    QtCreator::CppEditor
  DEPENDS
    Qt::Core
    Qt::Gui
    Qt::Quick
    Qt::Widgets
    Qt::Network
    Qt::Svg
    QtCreator::ExtensionSystem
    QtCreator::Utils
    QtCreator::CPlusPlus
    LLMQore
    Skills
    QodeAssistSession
    QodeAssistChatViewplugin
  SOURCES
    .github/workflows/build_cmake.yml
    .github/workflows/README.md
    README.md
    sources/plugin/qodeassist.cpp
    sources/plugin/QodeAssistConstants.hpp
    sources/plugin/QodeAssisttr.h
    sources/plugin/Version.hpp
    sources/plugin/ConfigurationManager.hpp sources/plugin/ConfigurationManager.cpp
    sources/plugin/UpdateStatusWidget.hpp sources/plugin/UpdateStatusWidget.cpp
    sources/llmcore/ContextData.hpp
    sources/llmcore/RequestType.hpp
    sources/completion/LLMClientInterface.hpp sources/completion/LLMClientInterface.cpp
    sources/completion/LSPCompletion.hpp
    sources/completion/LLMSuggestion.hpp sources/completion/LLMSuggestion.cpp
    sources/completion/QodeAssistClient.hpp sources/completion/QodeAssistClient.cpp
    sources/completion/CodeHandler.hpp sources/completion/CodeHandler.cpp
    sources/refactor/QuickRefactorHandler.hpp sources/refactor/QuickRefactorHandler.cpp
    sources/refactor/RefactorContextHelper.hpp
    sources/refactor/RefactorSuggestion.hpp sources/refactor/RefactorSuggestion.cpp
    sources/refactor/RefactorSuggestionHoverHandler.hpp sources/refactor/RefactorSuggestionHoverHandler.cpp
    sources/refactor/ResponseCleaner.hpp
    sources/templates/PromptTemplate.hpp
    sources/templates/PromptTemplateManager.hpp sources/templates/PromptTemplateManager.cpp
    sources/templates/IPromptProvider.hpp
    sources/templates/PromptProviderChat.hpp
    sources/templates/PromptProviderFim.hpp
    sources/templates/Templates.hpp
    sources/templates/CodeLlamaFim.hpp
    sources/templates/Ollama.hpp
    sources/templates/Claude.hpp
    sources/templates/OpenAI.hpp
    sources/templates/MistralAI.hpp
    sources/templates/StarCoder2Fim.hpp
    sources/templates/Qwen25CoderFIM.hpp
    sources/templates/OpenAICompatible.hpp
    sources/templates/Llama3.hpp
    sources/templates/ChatML.hpp
    sources/templates/Alpaca.hpp
    sources/templates/Llama2.hpp
    sources/templates/CodeLlamaQMLFim.hpp
    sources/templates/GoogleAI.hpp
    sources/templates/LlamaCppFim.hpp
    sources/templates/Qwen3CoderFIM.hpp
    sources/templates/OpenAIResponses.hpp
    sources/providers/Provider.hpp sources/providers/Provider.cpp
    sources/providers/ProvidersManager.hpp sources/providers/ProvidersManager.cpp
    sources/providers/IProviderRegistry.hpp
    sources/providers/ProviderID.hpp
    sources/providers/Providers.hpp
    sources/providers/ProviderUrlUtils.hpp
    sources/providers/OllamaProvider.hpp sources/providers/OllamaProvider.cpp
    sources/providers/OllamaCompatProvider.hpp sources/providers/OllamaCompatProvider.cpp
    sources/providers/ClaudeProvider.hpp sources/providers/ClaudeProvider.cpp
    sources/providers/OpenAIProvider.hpp sources/providers/OpenAIProvider.cpp
    sources/providers/MistralAIProvider.hpp sources/providers/MistralAIProvider.cpp
    sources/providers/LMStudioProvider.hpp sources/providers/LMStudioProvider.cpp
    sources/providers/LMStudioResponsesProvider.hpp sources/providers/LMStudioResponsesProvider.cpp
    sources/providers/OpenAICompatProvider.hpp sources/providers/OpenAICompatProvider.cpp
    sources/providers/OpenRouterAIProvider.hpp sources/providers/OpenRouterAIProvider.cpp
    sources/providers/GoogleAIProvider.hpp sources/providers/GoogleAIProvider.cpp
    sources/providers/LlamaCppProvider.hpp sources/providers/LlamaCppProvider.cpp
    sources/providers/CodestralProvider.hpp sources/providers/CodestralProvider.cpp
    sources/providers/OpenAIResponsesProvider.hpp sources/providers/OpenAIResponsesProvider.cpp
    sources/providers/QwenProvider.hpp sources/providers/QwenProvider.cpp
    sources/providers/QwenResponsesProvider.hpp sources/providers/QwenResponsesProvider.cpp
    sources/providers/DeepSeekProvider.hpp sources/providers/DeepSeekProvider.cpp
    QodeAssist.qrc
    sources/chat/ChatOutputPane.h sources/chat/ChatOutputPane.cpp
    sources/chat/NavigationPanel.hpp sources/chat/NavigationPanel.cpp
    sources/chat/ChatDocument.hpp sources/chat/ChatDocument.cpp
    sources/chat/ChatEditor.hpp sources/chat/ChatEditor.cpp
    sources/chat/ChatEditorFactory.hpp sources/chat/ChatEditorFactory.cpp
    sources/widgets/CompletionProgressHandler.hpp sources/widgets/CompletionProgressHandler.cpp
    sources/widgets/CompletionErrorHandler.hpp sources/widgets/CompletionErrorHandler.cpp
    sources/widgets/CompletionHintWidget.hpp sources/widgets/CompletionHintWidget.cpp
    sources/widgets/CompletionHintHandler.hpp sources/widgets/CompletionHintHandler.cpp
    sources/widgets/ProgressWidget.hpp sources/widgets/ProgressWidget.cpp
    sources/widgets/ErrorWidget.hpp sources/widgets/ErrorWidget.cpp
    sources/widgets/EditorChatButton.hpp sources/widgets/EditorChatButton.cpp
    sources/widgets/EditorChatButtonHandler.hpp sources/widgets/EditorChatButtonHandler.cpp
    sources/widgets/QuickRefactorDialog.hpp sources/widgets/QuickRefactorDialog.cpp
    sources/widgets/CustomInstructionsManager.hpp sources/widgets/CustomInstructionsManager.cpp
    sources/widgets/AddCustomInstructionDialog.hpp sources/widgets/AddCustomInstructionDialog.cpp
    sources/widgets/RefactorWidget.hpp sources/widgets/RefactorWidget.cpp
    sources/widgets/RefactorWidgetHandler.hpp sources/widgets/RefactorWidgetHandler.cpp
    sources/widgets/ContextExtractor.hpp
    sources/widgets/DiffStatistics.hpp
    sources/tools/ToolsRegistration.hpp sources/tools/ToolsRegistration.cpp
    sources/tools/ListProjectFilesTool.hpp sources/tools/ListProjectFilesTool.cpp
    sources/tools/GetIssuesListTool.hpp sources/tools/GetIssuesListTool.cpp
    sources/tools/CreateNewFileTool.hpp sources/tools/CreateNewFileTool.cpp
    sources/tools/EditFileTool.hpp sources/tools/EditFileTool.cpp
    sources/tools/BuildProjectTool.hpp sources/tools/BuildProjectTool.cpp
    sources/tools/ExecuteTerminalCommandTool.hpp sources/tools/ExecuteTerminalCommandTool.cpp
    sources/tools/ProjectSearchTool.hpp sources/tools/ProjectSearchTool.cpp
    sources/tools/FindFileTool.hpp sources/tools/FindFileTool.cpp
    sources/tools/ReadFileTool.hpp sources/tools/ReadFileTool.cpp
    sources/tools/FileSearchUtils.hpp sources/tools/FileSearchUtils.cpp
    sources/tools/TodoTool.hpp sources/tools/TodoTool.cpp
    sources/tools/ReadOriginalHistoryTool.hpp sources/tools/ReadOriginalHistoryTool.cpp
    sources/tools/SkillTool.hpp sources/tools/SkillTool.cpp
    sources/mcp/McpServerManager.hpp sources/mcp/McpServerManager.cpp
    sources/mcp/McpServerConnection.hpp sources/mcp/McpServerConnection.cpp
    sources/mcp/McpClientsManager.hpp sources/mcp/McpClientsManager.cpp
    sources/settings/McpClientsListAspect.hpp sources/settings/McpClientsListAspect.cpp
)

target_include_directories(QodeAssist PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/sources)

extend_qtc_plugin(QodeAssist
  CONDITION WITH_TESTS
  DEPENDS Qt::Test
  SOURCES
    tests/QodeAssistTest.hpp tests/QodeAssistTest.cpp
)

if(WITH_TESTS)
  target_include_directories(QodeAssist PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()

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

#TODO change to TS_OUTPUT_DIRECTORY after removing Qt6.8
qt_add_translations(TARGETS QodeAssist
          TS_FILE_DIR ${CMAKE_CURRENT_LIST_DIR}/resources/translations
          RESOURCE_PREFIX "/translations"
          LUPDATE_OPTIONS -no-obsolete -locations none
)
