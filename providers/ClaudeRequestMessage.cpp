// ClaudeRequestMessage.cpp
#include "ClaudeRequestMessage.hpp"

namespace QodeAssist::Providers {

ClaudeRequestMessage::ClaudeRequestMessage(const QJsonObject &originalRequest)
    : m_baseRequest(originalRequest)
    , m_messages(originalRequest["messages"].toArray())
{}

QJsonObject ClaudeRequestMessage::createFollowUpRequest(
    const QJsonObject &baseRequest,
    const QJsonArray &messages,
    const QString &assistantText,
    const QList<QPair<QString, QJsonObject>> &toolCalls,
    const QList<QPair<QString, QString>> &toolResults)
{
    QJsonArray newMessages = messages;

    // Добавляем assistant message с tool_use
    QJsonObject assistantMessage;
    assistantMessage["role"] = "assistant";
    QJsonArray content;

    if (!assistantText.isEmpty()) {
        content.append(QJsonObject{{"type", "text"}, {"text", assistantText}});
    }

    for (const auto &toolCall : toolCalls) {
        content.append(
            QJsonObject{
                {"type", "tool_use"},
                {"id", toolCall.first},
                {"name", toolCall.second["name"].toString()},
                {"input", toolCall.second["input"]}});
    }

    assistantMessage["content"] = content;
    newMessages.append(assistantMessage);

    // Добавляем user message с tool_result
    QJsonObject userMessage;
    userMessage["role"] = "user";
    QJsonArray toolResultsArray;

    for (const auto &result : toolResults) {
        toolResultsArray.append(
            QJsonObject{
                {"type", "tool_result"}, {"tool_use_id", result.first}, {"content", result.second}});
    }

    userMessage["content"] = toolResultsArray;
    newMessages.append(userMessage);

    QJsonObject result = baseRequest;
    result["messages"] = newMessages;
    return result;
}

QJsonObject ClaudeRequestMessage::toJson() const
{
    QJsonObject result = m_baseRequest;
    result["messages"] = m_messages;
    return result;
}

} // namespace QodeAssist::Providers
