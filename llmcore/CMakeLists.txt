add_library(LLMCore STATIC
    RequestType.hpp
    Provider.hpp Provider.cpp
    ProvidersManager.hpp ProvidersManager.cpp
    ContextData.hpp
    IPromptProvider.hpp
    IProviderRegistry.hpp
    PromptProviderChat.hpp
    PromptProviderFim.hpp
    PromptTemplate.hpp
    PromptTemplateManager.hpp PromptTemplateManager.cpp
    RequestConfig.hpp
    ValidationUtils.hpp ValidationUtils.cpp
    ProviderID.hpp
    HttpClient.hpp HttpClient.cpp
    BaseClient.hpp BaseClient.cpp
    ClaudeClient.hpp ClaudeClient.cpp
    ClaudeMessage.hpp ClaudeMessage.cpp
    OpenAIMessage.hpp OpenAIMessage.cpp
    OpenAIClient.hpp OpenAIClient.cpp
    OllamaMessage.hpp OllamaMessage.cpp
    OllamaClient.hpp OllamaClient.cpp
    GoogleMessage.hpp GoogleMessage.cpp
    GoogleAIClient.hpp GoogleAIClient.cpp
    OpenAIResponsesMessage.hpp OpenAIResponsesMessage.cpp
    OpenAIResponsesClient.hpp OpenAIResponsesClient.cpp
    DataBuffers.hpp
    SSEUtils.hpp
    SSEBuffer.hpp SSEBuffer.cpp
    BaseTool.hpp BaseTool.cpp
    ToolExceptions.hpp
    ToolHandler.hpp ToolHandler.cpp
    ToolsManager.hpp ToolsManager.cpp
    ContentBlocks.hpp
    RulesLoader.hpp RulesLoader.cpp
    ResponseCleaner.hpp
)

target_link_libraries(LLMCore
    PUBLIC
    Qt::Core
    Qt::Network
    QtCreator::Core
    QtCreator::Utils
    QtCreator::ExtensionSystem
    PRIVATE
    QodeAssistLogger
)

target_include_directories(LLMCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
