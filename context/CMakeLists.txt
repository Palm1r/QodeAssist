add_library(Context STATIC
    DocumentContextReader.hpp DocumentContextReader.cpp
    ChangesManager.h ChangesManager.cpp
    ContextManager.hpp ContextManager.cpp
    ContentFile.hpp
    TokenUtils.hpp TokenUtils.cpp
    ProgrammingLanguage.hpp ProgrammingLanguage.cpp
    RAGManager.hpp RAGManager.cpp
    RAGStorage.hpp RAGStorage.cpp
    RAGData.hpp
    RAGVectorizer.hpp RAGVectorizer.cpp
    RAGSimilaritySearch.hpp RAGSimilaritySearch.cpp
)

target_link_libraries(Context
    PUBLIC
    Qt::Core
    Qt::Sql
    QtCreator::Core
    QtCreator::TextEditor
    QtCreator::Utils
    QtCreator::ProjectExplorer
    PRIVATE
    LLMCore
    QodeAssistSettings
)

target_include_directories(Context PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
