add_library(LLMCore STATIC
    Provider.hpp
    ProvidersManager.hpp ProvidersManager.cpp
    ContextData.hpp
    PromptTemplate.hpp
    PromptTemplateManager.hpp PromptTemplateManager.cpp
    RequestConfig.hpp
    RequestHandler.hpp RequestHandler.cpp
)

target_include_directories(LLMCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

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
