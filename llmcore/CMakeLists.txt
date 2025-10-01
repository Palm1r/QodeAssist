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
    DataBuffers.hpp
    SSEBuffer.hpp SSEBuffer.cpp
    BaseTool.hpp
    BaseTool.cpp
    ContentBlocks.hpp
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
