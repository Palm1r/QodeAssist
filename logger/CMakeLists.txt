add_library(QodeAssistLogger STATIC
    Logger.cpp
    Logger.hpp
)

target_link_libraries(QodeAssistLogger
    PUBLIC
    Qt::Core
    QtCreator::Core
)

target_include_directories(QodeAssistLogger
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
