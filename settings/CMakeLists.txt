add_library(QodeAssistSettings STATIC
    GeneralSettings.hpp GeneralSettings.cpp
    CustomPromptSettings.hpp CustomPromptSettings.cpp
    SettingsUtils.hpp
    SettingsConstants.hpp
    ButtonAspect.hpp
    SettingsTr.hpp
    CodeCompletionSettings.hpp CodeCompletionSettings.cpp
    ChatAssistantSettings.hpp ChatAssistantSettings.cpp
    SettingsDialog.hpp SettingsDialog.cpp
    ProjectSettings.hpp ProjectSettings.cpp
    ProjectSettingsPanel.hpp ProjectSettingsPanel.cpp
    ProviderSettings.hpp ProviderSettings.cpp
    PluginUpdater.hpp PluginUpdater.cpp
    UpdateDialog.hpp UpdateDialog.cpp
)

target_link_libraries(QodeAssistSettings
    PUBLIC
    Qt::Core
    Qt::Network
    QtCreator::Core
    QtCreator::Utils
    QodeAssistLogger
)
target_include_directories(QodeAssistSettings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
