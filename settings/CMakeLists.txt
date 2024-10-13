add_library(QodeAssistSettings STATIC
    GeneralSettings.hpp GeneralSettings.cpp
    ContextSettings.hpp ContextSettings.cpp
    CustomPromptSettings.hpp CustomPromptSettings.cpp
    PresetPromptsSettings.hpp PresetPromptsSettings.cpp
    SettingsUtils.hpp
    SettingsConstants.hpp
)

target_link_libraries(QodeAssistSettings
    PUBLIC
    Qt::Core
    Qt::Network
    QtCreator::Core
    QtCreator::Utils
    QodeAssistLogger
    PRIVATE
    LLMCore
)
target_include_directories(QodeAssistSettings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
