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
    RequestHandlerBase.hpp RequestHandlerBase.cpp
    RequestHandler.hpp RequestHandler.cpp
    OllamaMessage.hpp OllamaMessage.cpp
    OpenAIMessage.hpp OpenAIMessage.cpp
    ValidationUtils.hpp ValidationUtils.cpp
    ProviderID.hpp
    HttpClient.hpp HttpClient.cpp
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
