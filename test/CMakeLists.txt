add_executable(QodeAssistTest
    ../CodeHandler.cpp
    CodeHandlerTest.cpp
    DocumentContextReaderTest.cpp
    unittest_main.cpp
)

target_link_libraries(QodeAssistTest PRIVATE
    Qt::Core
    GTest::GTest
    GTest::Main
    QtCreator::LanguageClient
    Context
)

add_test(NAME QodeAssistTest COMMAND QodeAssistTest)
