add_library(TomlSerializer STATIC
    TomlWriter.hpp TomlWriter.cpp
)

target_link_libraries(TomlSerializer
    PUBLIC
        Qt::Core
)

target_include_directories(TomlSerializer
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
