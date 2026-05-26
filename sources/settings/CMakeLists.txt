add_library(QodeAssistAgentPipelines OBJECT
    AgentPipelinesPage.hpp AgentPipelinesPage.cpp
    PipelinesConfig.hpp PipelinesConfig.cpp
    AgentRosterWidget.hpp AgentRosterWidget.cpp
    AgentSlotWidget.hpp AgentSlotWidget.cpp
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
)

target_compile_definitions(QodeAssistAgentPipelines PRIVATE
    QODEASSIST_QT_CREATOR_VERSION_MAJOR=${QODEASSIST_QT_CREATOR_VERSION_MAJOR}
    QODEASSIST_QT_CREATOR_VERSION_MINOR=${QODEASSIST_QT_CREATOR_VERSION_MINOR}
    QODEASSIST_QT_CREATOR_VERSION_PATCH=${QODEASSIST_QT_CREATOR_VERSION_PATCH}
)
