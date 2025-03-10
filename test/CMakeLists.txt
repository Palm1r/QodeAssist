add_executable(QodeAssistTest
    ../CodeHandler.cpp
    ../LLMClientInterface.cpp
    CodeHandlerTest.cpp
    DocumentContextReaderTest.cpp
    LLMClientInterfaceTests.cpp
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
)

target_compile_definitions(QodeAssistTest PRIVATE CMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_test(NAME QodeAssistTest COMMAND QodeAssistTest)
