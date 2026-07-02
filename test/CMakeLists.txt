add_executable(QodeAssistTest
    ../CodeHandler.cpp
    ../LLMSuggestion.cpp
    ../sources/settings/PipelinesConfig.cpp
    CodeHandlerTest.cpp
    DocumentContextReaderTest.cpp
    EnvBlockFormatterTest.cpp
    LLMSuggestionTest.cpp
    JsonPromptTemplateTest.cpp
    ContextAssemblerTest.cpp
    ResponseRouterTest.cpp
    BundledAgentsTest.cpp
    AgentLoaderTest.cpp
    AgentConfigTest.cpp
    AgentRouterTest.cpp
    ClaudeCacheControlTest.cpp
    ContextRendererTest.cpp
    ErrorInfoTest.cpp
    MessageSerializerTest.cpp
    PipelinesConfigTest.cpp
    ResponseCleanerTest.cpp
    SystemPromptBuilderTest.cpp
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
    TomlSerializer
    tomlplusplus::tomlplusplus
    QodeAssistLogger
)

target_include_directories(QodeAssistTest PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/sources/settings
    ${CMAKE_SOURCE_DIR}/sources/tomlSerializer
    ${CMAKE_SOURCE_DIR}/logger
)

target_compile_definitions(QodeAssistTest PRIVATE CMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_test(NAME QodeAssistTest COMMAND QodeAssistTest)
