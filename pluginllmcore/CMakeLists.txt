add_library(PluginLLMCore STATIC
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
    DataBuffers.hpp
    SSEBuffer.hpp SSEBuffer.cpp
    BaseTool.hpp BaseTool.cpp
    ContentBlocks.hpp
    RulesLoader.hpp RulesLoader.cpp
    ResponseCleaner.hpp
)

target_link_libraries(PluginLLMCore
    PUBLIC
    Qt::Core
    Qt::Network
    QtCreator::Core
    QtCreator::Utils
    QtCreator::ExtensionSystem
    LLMCore
    PRIVATE
    QodeAssistLogger
)

target_include_directories(PluginLLMCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
