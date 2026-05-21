add_library(QodeAssistSettings STATIC
    GeneralSettings.hpp GeneralSettings.cpp
    ConfigurationManager.hpp ConfigurationManager.cpp
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
    SettingsDialog.hpp SettingsDialog.cpp
    ProjectSettings.hpp ProjectSettings.cpp
    ProjectSettingsPanel.hpp ProjectSettingsPanel.cpp
    ProviderSettings.hpp ProviderSettings.cpp
    ProviderNameMigration.hpp
    ProvidersSettingsPage.hpp ProvidersSettingsPage.cpp
    ProvidersSettingsHelpers.hpp
    SectionBox.hpp SectionBox.cpp
    ProviderListItem.hpp ProviderListItem.cpp
    ProviderDetailPane.hpp ProviderDetailPane.cpp
    NewProviderDialog.hpp NewProviderDialog.cpp
    PluginUpdater.hpp PluginUpdater.cpp
    UpdateDialog.hpp UpdateDialog.cpp
    AgentRole.hpp AgentRole.cpp
    AgentRoleDialog.hpp AgentRoleDialog.cpp
    AgentRolesWidget.hpp AgentRolesWidget.cpp
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
    Skills
)
target_include_directories(QodeAssistSettings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
