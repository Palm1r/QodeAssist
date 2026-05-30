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
find_package(Qt6 COMPONENTS Core Gui Quick Widgets Network Svg Test LinguistTools REQUIRED)
find_package(GTest)

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
add_subdirectory(logger)
add_subdirectory(settings)
add_subdirectory(UIControls)
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
    ProvidersConfig
    Agents
    Skills
    QodeAssistChatViewplugin
  SOURCES
    .github/workflows/build_cmake.yml
    .github/workflows/README.md
    README.md
    qodeassist.cpp
    QodeAssistConstants.hpp
    QodeAssisttr.h
    LLMClientInterface.hpp LLMClientInterface.cpp
    RefactorContextHelper.hpp
    QodeAssist.qrc
    LSPCompletion.hpp
    LLMSuggestion.hpp LLMSuggestion.cpp
    RefactorSuggestion.hpp RefactorSuggestion.cpp
    RefactorSuggestionHoverHandler.hpp RefactorSuggestionHoverHandler.cpp
    QodeAssistClient.hpp QodeAssistClient.cpp
    chat/ChatOutputPane.h chat/ChatOutputPane.cpp
    chat/NavigationPanel.hpp chat/NavigationPanel.cpp
    chat/ChatDocument.hpp chat/ChatDocument.cpp
    chat/ChatEditor.hpp chat/ChatEditor.cpp
    chat/ChatEditorFactory.hpp chat/ChatEditorFactory.cpp
    CodeHandler.hpp CodeHandler.cpp
    UpdateStatusWidget.hpp UpdateStatusWidget.cpp
    widgets/CompletionProgressHandler.hpp widgets/CompletionProgressHandler.cpp
    widgets/CompletionErrorHandler.hpp widgets/CompletionErrorHandler.cpp
    widgets/ProgressWidget.hpp widgets/ProgressWidget.cpp
    widgets/ErrorWidget.hpp widgets/ErrorWidget.cpp
    widgets/QuickRefactorDialog.hpp widgets/QuickRefactorDialog.cpp
    widgets/CustomInstructionsManager.hpp widgets/CustomInstructionsManager.cpp
    widgets/AddCustomInstructionDialog.hpp widgets/AddCustomInstructionDialog.cpp
    widgets/RefactorWidget.hpp widgets/RefactorWidget.cpp
    widgets/RefactorWidgetHandler.hpp widgets/RefactorWidgetHandler.cpp
    widgets/ContextExtractor.hpp
    widgets/DiffStatistics.hpp

    QuickRefactorHandler.hpp QuickRefactorHandler.cpp
    tools/ToolsRegistration.hpp tools/ToolsRegistration.cpp
    tools/ListProjectFilesTool.hpp tools/ListProjectFilesTool.cpp
    tools/GetIssuesListTool.hpp tools/GetIssuesListTool.cpp
    tools/CreateNewFileTool.hpp tools/CreateNewFileTool.cpp
    tools/EditFileTool.hpp tools/EditFileTool.cpp
    tools/BuildProjectTool.hpp tools/BuildProjectTool.cpp
    tools/ExecuteTerminalCommandTool.hpp tools/ExecuteTerminalCommandTool.cpp
    tools/ProjectSearchTool.hpp tools/ProjectSearchTool.cpp
    tools/FindFileTool.hpp tools/FindFileTool.cpp
    tools/ReadFileTool.hpp tools/ReadFileTool.cpp
    tools/FileSearchUtils.hpp tools/FileSearchUtils.cpp
    tools/TodoTool.hpp tools/TodoTool.cpp
    tools/ReadOriginalHistoryTool.hpp tools/ReadOriginalHistoryTool.cpp
    tools/SkillTool.hpp tools/SkillTool.cpp
    mcp/McpServerManager.hpp mcp/McpServerManager.cpp
    mcp/McpServerConnection.hpp mcp/McpServerConnection.cpp
    mcp/McpClientsManager.hpp mcp/McpClientsManager.cpp
    settings/McpClientsListAspect.hpp settings/McpClientsListAspect.cpp
)

target_link_libraries(QodeAssist PRIVATE QodeAssistAgentPipelines Session)

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
