add_library(Common INTERFACE)

target_sources(Common INTERFACE
    ContextData.hpp
)

target_include_directories(Common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Common INTERFACE
    Qt::Core
)
