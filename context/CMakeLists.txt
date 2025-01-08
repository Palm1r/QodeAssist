add_library(Context STATIC
    DocumentContextReader.hpp DocumentContextReader.cpp
    ChangesManager.h ChangesManager.cpp
    ContextManager.hpp ContextManager.cpp
    ContentFile.hpp
)

target_link_libraries(Context
    PUBLIC
    Qt::Core
    QtCreator::Core
    QtCreator::TextEditor
    QtCreator::Utils
    QtCreator::ProjectExplorer
    PRIVATE
    LLMCore
    QodeAssistSettings
)

target_include_directories(Context PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
