add_library(QodeAssistSettings STATIC
    GeneralSettings.hpp GeneralSettings.cpp
    CustomPromptSettings.hpp CustomPromptSettings.cpp
    ConfigurationManager.hpp ConfigurationManager.cpp
    SettingsUtils.hpp
    SettingsConstants.hpp
    ButtonAspect.hpp
    SettingsTr.hpp
    CodeCompletionSettings.hpp CodeCompletionSettings.cpp
    ChatAssistantSettings.hpp ChatAssistantSettings.cpp
    QuickRefactorSettings.hpp QuickRefactorSettings.cpp
    ToolsSettings.hpp ToolsSettings.cpp
    SettingsDialog.hpp SettingsDialog.cpp
    ProjectSettings.hpp ProjectSettings.cpp
    ProjectSettingsPanel.hpp ProjectSettingsPanel.cpp
    ProviderSettings.hpp ProviderSettings.cpp
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
)
target_include_directories(QodeAssistSettings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
