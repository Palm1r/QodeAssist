add_library(LLMCore STATIC
    # core
    core/RequestType.hpp
    core/ContextData.hpp
    core/RequestConfig.hpp
    core/ContentBlocks.hpp
    core/DataBuffers.hpp
    core/Provider.hpp core/Provider.cpp
    core/ProviderID.hpp
    core/ProvidersManager.hpp core/ProvidersManager.cpp
    core/IProviderRegistry.hpp
    core/IPromptProvider.hpp
    core/Log.hpp core/Log.cpp

    # prompt
    prompt/PromptTemplate.hpp
    prompt/PromptTemplateManager.hpp prompt/PromptTemplateManager.cpp
    prompt/ValidationUtils.hpp prompt/ValidationUtils.cpp

    # network
    network/HttpClient.hpp network/HttpClient.cpp
    network/BaseClient.hpp network/BaseClient.cpp
    network/SSEUtils.hpp
    network/SSEBuffer.hpp network/SSEBuffer.cpp
    network/RequestHandlerBase.hpp network/RequestHandlerBase.cpp

    # clients/claude
    clients/claude/ClaudeClient.hpp clients/claude/ClaudeClient.cpp
    clients/claude/ClaudeMessage.hpp clients/claude/ClaudeMessage.cpp

    # clients/openai
    clients/openai/OpenAIClient.hpp clients/openai/OpenAIClient.cpp
    clients/openai/OpenAIMessage.hpp clients/openai/OpenAIMessage.cpp
    clients/openai/OpenAIResponsesClient.hpp clients/openai/OpenAIResponsesClient.cpp
    clients/openai/OpenAIResponsesMessage.hpp clients/openai/OpenAIResponsesMessage.cpp

    # clients/ollama
    clients/ollama/OllamaClient.hpp clients/ollama/OllamaClient.cpp
    clients/ollama/OllamaMessage.hpp clients/ollama/OllamaMessage.cpp

    # clients/google
    clients/google/GoogleAIClient.hpp clients/google/GoogleAIClient.cpp
    clients/google/GoogleMessage.hpp clients/google/GoogleMessage.cpp

    # tools
    tools/BaseTool.hpp tools/BaseTool.cpp
    tools/ToolExceptions.hpp
    tools/ToolHandler.hpp tools/ToolHandler.cpp
    tools/ToolsManager.hpp tools/ToolsManager.cpp
)

target_link_libraries(LLMCore
    PRIVATE
    Qt::Core
    Qt::Network
    Qt::Concurrent
)

target_include_directories(LLMCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
