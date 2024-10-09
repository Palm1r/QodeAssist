find_package(QtCreator REQUIRED COMPONENTS Core Utils ExtensionSystem)
find_package(Qt6 COMPONENTS Core Gui Widgets Network REQUIRED)

add_library(LLMCore STATIC
    Logger.hpp
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
    PRIVATE
    Qt::Core
    Qt::Network
    QtCreator::Core
    QtCreator::Utils
    QtCreator::ExtensionSystem
)

set_target_properties(LLMCore PROPERTIES
    PUBLIC_HEADER
    "Provider.hpp;ProvidersManager.hpp;ContextData.hpp;PromptTemplate.hpp;PromptTemplateManager.hpp;RequestConfig.hpp;RequestHandler.hpp"
)
