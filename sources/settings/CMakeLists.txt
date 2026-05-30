add_library(QodeAssistAgentPipelines STATIC
    PipelinesConfig.hpp PipelinesConfig.cpp
    Pill.hpp Pill.cpp
    AgentRosterWidget.hpp AgentRosterWidget.cpp
    AgentSelectionDialog.hpp AgentSelectionDialog.cpp
)

target_include_directories(QodeAssistAgentPipelines PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/sources/providers
    ${CMAKE_SOURCE_DIR}/sources/common
    ${CMAKE_SOURCE_DIR}/sources/agents
    ${CMAKE_SOURCE_DIR}/settings
    ${CMAKE_SOURCE_DIR}/logger
)

target_link_libraries(QodeAssistAgentPipelines PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::Network
    QtCreator::Core
    QtCreator::Utils
    Agents
    Providers
    Common
    LLMQore
    QodeAssistLogger
    TomlSerializer
    tomlplusplus::tomlplusplus
    QodeAssistSettings
)

target_compile_definitions(QodeAssistAgentPipelines PRIVATE
    QODEASSIST_QT_CREATOR_VERSION_MAJOR=${QODEASSIST_QT_CREATOR_VERSION_MAJOR}
    QODEASSIST_QT_CREATOR_VERSION_MINOR=${QODEASSIST_QT_CREATOR_VERSION_MINOR}
    QODEASSIST_QT_CREATOR_VERSION_PATCH=${QODEASSIST_QT_CREATOR_VERSION_PATCH}
)
