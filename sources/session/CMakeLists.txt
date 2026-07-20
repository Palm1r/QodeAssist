add_library(QodeAssistSession STATIC
    ContentBlock.hpp
    Message.hpp
    ConversationHistory.hpp ConversationHistory.cpp
    FencedText.hpp FencedText.cpp
    FileEditPayload.hpp FileEditPayload.cpp
    HistoryProjection.hpp HistoryProjection.cpp
    HistorySerializer.hpp HistorySerializer.cpp
    SessionEvent.hpp SessionEvent.cpp
    TurnRequest.hpp
    ChatBackend.hpp
    Session.hpp Session.cpp
    TurnContext.hpp TurnContext.cpp
    TurnContextPorts.hpp
    TurnContextBuilder.hpp TurnContextBuilder.cpp
)

target_link_libraries(QodeAssistSession
    PUBLIC
    Qt::Core
)

target_include_directories(QodeAssistSession PUBLIC ${CMAKE_SOURCE_DIR}/sources)
