add_library(QodeAssistLogger STATIC
    EmptyRequestPerformanceLogger.hpp
    IRequestPerformanceLogger.hpp
    Logger.cpp
    Logger.hpp
    RequestPerformanceLogger.hpp RequestPerformanceLogger.cpp
)

target_link_libraries(QodeAssistLogger
    PUBLIC
    Qt::Core
    QtCreator::Core
)

target_include_directories(QodeAssistLogger
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
