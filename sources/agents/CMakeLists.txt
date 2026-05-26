add_library(Agents STATIC
    AgentConfig.hpp
    Agent.hpp Agent.cpp
    AgentLoader.hpp AgentLoader.cpp
    AgentFactory.hpp AgentFactory.cpp
    AgentRouter.hpp AgentRouter.cpp
    ContextRenderer.hpp ContextRenderer.cpp
    agents.qrc
)

target_link_libraries(Agents
    PUBLIC
        Qt::Core
        Qt::Network
        QtCreator::Core
        QtCreator::Utils
        LLMQore
        pantor::inja
        ProvidersConfig
        Common
        Providers
        Templates
    PRIVATE
        QodeAssistLogger
        tomlplusplus::tomlplusplus
)

target_include_directories(Agents
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
