add_library(QodeAssistSettings STATIC
    GeneralSettings.hpp GeneralSettings.cpp
    SettingsUtils.hpp
    SettingsConstants.hpp
    ButtonAspect.hpp
    SettingsTr.hpp
    CodeCompletionSettings.hpp CodeCompletionSettings.cpp
    ChatAssistantSettings.hpp ChatAssistantSettings.cpp
    QuickRefactorSettings.hpp QuickRefactorSettings.cpp
    ToolsSettings.hpp ToolsSettings.cpp
    SkillsSettings.hpp SkillsSettings.cpp
    McpSettings.hpp McpSettings.cpp
    ProjectSettings.hpp ProjectSettings.cpp
    ProjectSettingsPanel.hpp ProjectSettingsPanel.cpp
    ProviderSettings.hpp ProviderSettings.cpp
    ProvidersSettingsPage.hpp ProvidersSettingsPage.cpp
    SettingsTheme.hpp
    SettingsUiBuilders.hpp SettingsUiBuilders.cpp
    SectionBox.hpp SectionBox.cpp
    TagChip.hpp TagChip.cpp
    ProviderListItem.hpp ProviderListItem.cpp
    ProviderDetailPane.hpp ProviderDetailPane.cpp
    PluginUpdater.hpp PluginUpdater.cpp
    UpdateDialog.hpp UpdateDialog.cpp
    AgentsSettingsPage.hpp AgentsSettingsPage.cpp
    AgentDetailPane.hpp AgentDetailPane.cpp
    ModelSelector.hpp ModelSelector.cpp
    AgentListItem.hpp AgentListItem.cpp
    AgentListPane.hpp AgentListPane.cpp
    AgentDuplicator.hpp AgentDuplicator.cpp
    TagFilterStrip.hpp TagFilterStrip.cpp
    CollapsibleHeader.hpp CollapsibleHeader.cpp
)

target_link_libraries(QodeAssistSettings
    PUBLIC
    Qt::Core
    Qt::Widgets
    Qt::Network
    QtCreator::Core
    QtCreator::Utils
    QodeAssistLogger
    ProvidersConfig
    Agents
    Skills
    QodeAssistAgentPipelines
)
target_include_directories(QodeAssistSettings
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/sources/settings
)
