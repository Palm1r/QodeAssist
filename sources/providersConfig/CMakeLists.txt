add_library(ProvidersConfig STATIC
    ProviderInstance.hpp ProviderInstance.cpp
    ProviderInstanceLoader.hpp ProviderInstanceLoader.cpp
    ProviderInstanceWriter.hpp ProviderInstanceWriter.cpp
    ProviderInstanceFactory.hpp ProviderInstanceFactory.cpp
    ProviderSecretsStore.hpp ProviderSecretsStore.cpp
    ProviderLauncher.hpp ProviderLauncher.cpp

    provider_instances.qrc
)

target_link_libraries(ProvidersConfig
    PUBLIC
        Qt::Core
        Qt::Network
        QtCreator::Core
        QtCreator::Utils
        QtCreator::TerminalLib
    PRIVATE
        QodeAssistLogger
        TomlSerializer
        tomlplusplus::tomlplusplus
)

target_include_directories(ProvidersConfig
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
