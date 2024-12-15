add_library(LLMCore STATIC
    RequestType.hpp
    Provider.hpp
    ProvidersManager.hpp ProvidersManager.cpp
    ContextData.hpp
    PromptTemplate.hpp
    PromptTemplateManager.hpp PromptTemplateManager.cpp
    RequestConfig.hpp
    RequestHandler.hpp RequestHandler.cpp
    OllamaMessage.hpp OllamaMessage.cpp
    OpenAIMessage.hpp OpenAIMessage.cpp
    ValidationUtils.hpp ValidationUtils.cpp
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
