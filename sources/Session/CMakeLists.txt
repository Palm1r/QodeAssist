add_library(Session STATIC
    Message.hpp Message.cpp
    MessageSerializer.hpp MessageSerializer.cpp
    PluginBlocks.hpp
    LLMRequest.hpp
    ErrorInfo.hpp
    ResponseEvent.hpp
    ConversationHistory.hpp ConversationHistory.cpp
    ResponseRouter.hpp ResponseRouter.cpp
    Session.hpp Session.cpp
    SessionManager.hpp SessionManager.cpp
    SystemPromptBuilder.hpp SystemPromptBuilder.cpp
    ToolContributorRegistry.hpp
)

target_link_libraries(Session
    PUBLIC
        Qt::Core
        LLMQore
        Agents
)

target_include_directories(Session PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
