find_package(QtCreator REQUIRED COMPONENTS Core)

add_library(QodeAssistSettings STATIC
    GeneralSettings.hpp GeneralSettings.cpp
    ContextSettings.hpp ContextSettings.cpp
    CustomPromptSettings.hpp CustomPromptSettings.cpp
    PresetPromptsSettings.hpp PresetPromptsSettings.cpp
    SettingsUtils.hpp
    SettingsConstants.hpp
)

target_include_directories(QodeAssistSettings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(QodeAssistSettings
    PRIVATE
    Qt::Core
    Qt::Network
    QtCreator::Core
    QtCreator::Utils
    LLMCore
    QodeAssistLogger
)

set_target_properties(QodeAssistSettings PROPERTIES
    PUBLIC_HEADER
    "GeneralSettings.hpp;ContextSettings.hpp;CustomPromptSettings.hpp;PresetPromptsSettings.hpp;SettingsUtils.hpp"
)
