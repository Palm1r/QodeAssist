add_library(Templates STATIC
    PromptTemplate.hpp
    JsonPromptTemplate.hpp JsonPromptTemplate.cpp
)

target_link_libraries(Templates
    PUBLIC
        Qt::Core
        Common
        Providers
        pantor::inja
)

target_include_directories(Templates
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/sources/agents
)
