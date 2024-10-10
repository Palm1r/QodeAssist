find_package(Qt6 COMPONENTS Core REQUIRED)
find_package(QtCreator REQUIRED COMPONENTS Core)

add_library(QodeAssistLogger STATIC
    Logger.cpp
    Logger.hpp
)

target_link_libraries(QodeAssistLogger
    PRIVATE
    Qt::Core
    QtCreator::Core
)

target_include_directories(QodeAssistLogger
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
