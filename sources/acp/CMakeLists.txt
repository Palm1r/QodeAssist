add_library(QodeAssistAcp STATIC
    AgentBinding.hpp AgentBinding.cpp
    AgentDefinition.hpp AgentDefinition.cpp
    AgentRegistryParser.hpp AgentRegistryParser.cpp
    AgentCatalog.hpp AgentCatalog.cpp
    AgentLaunch.hpp AgentLaunch.cpp
    AgentCatalogStore.hpp AgentCatalogStore.cpp
    AgentSpawn.hpp AgentSpawn.cpp
    AgentTester.hpp AgentTester.cpp
    ChatPermissionProvider.hpp ChatPermissionProvider.cpp
    AcpChatBackend.hpp AcpChatBackend.cpp
)

target_link_libraries(QodeAssistAcp
    PUBLIC
    Qt::Core
    Qt::Network
    QtCreator::Core
    QtCreator::Utils
    QtCreator::ProjectExplorer
    QtCreator::ExtensionSystem
    LLMQore
    QodeAssistSession
    PRIVATE
    QodeAssistLogger
    QodeAssistSettings
)

target_include_directories(QodeAssistAcp PUBLIC ${CMAKE_SOURCE_DIR}/sources)
