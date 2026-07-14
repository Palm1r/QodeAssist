add_executable(QodeAssistTest
    ../sources/completion/CodeHandler.cpp
    ../sources/completion/LLMClientInterface.cpp
    ../sources/completion/LLMSuggestion.cpp
    ../sources/providers/Provider.cpp
    CodeHandlerTest.cpp
    ClaudeCacheControlTest.cpp
    DocumentContextReaderTest.cpp
    LLMSuggestionTest.cpp
    # LLMClientInterfaceTests.cpp
    unittest_main.cpp
)

target_link_libraries(QodeAssistTest PRIVATE
    Qt::Core
    Qt::Test
    GTest::GTest
    GTest::gmock
    GTest::Main
    QtCreator::LanguageClient
    Context
    QodeAssistLogger
    LLMQore
)

target_include_directories(QodeAssistTest PRIVATE ${CMAKE_SOURCE_DIR}/sources)

target_compile_definitions(QodeAssistTest PRIVATE CMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_test(NAME QodeAssistTest COMMAND QodeAssistTest)
