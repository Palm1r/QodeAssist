add_library(Providers STATIC
    ProviderID.hpp
    Provider.hpp Provider.cpp
    ProviderFactory.hpp ProviderFactory.cpp
)

target_link_libraries(Providers
    PUBLIC
        Qt::Core
        Qt::Network
        QtCreator::Core
        QtCreator::Utils
        LLMQore
        Common
    PRIVATE
        QodeAssistLogger
)

target_include_directories(Providers
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/sources/templates
)
