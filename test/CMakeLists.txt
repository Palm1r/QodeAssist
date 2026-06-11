add_executable(QodeAssistTest
    ../CodeHandler.cpp
    ../LLMSuggestion.cpp
    CodeHandlerTest.cpp
    DocumentContextReaderTest.cpp
    LLMSuggestionTest.cpp
    JsonPromptTemplateTest.cpp
    ResponseRouterTest.cpp
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
    Common
    LLMQore
    Templates
    Agents
    Session
)

target_include_directories(QodeAssistTest PRIVATE ${CMAKE_SOURCE_DIR})

target_compile_definitions(QodeAssistTest PRIVATE CMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_test(NAME QodeAssistTest COMMAND QodeAssistTest)
