// ClaudeRequestMessage.hpp
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace QodeAssist::Providers {

class ClaudeRequestMessage
{
public:
    explicit ClaudeRequestMessage(const QJsonObject &originalRequest);

    // Статические методы для создания запросов
    static QJsonObject createFollowUpRequest(
        const QJsonObject &baseRequest,
        const QJsonArray &messages,
        const QString &assistantText,
        const QList<QPair<QString, QJsonObject>> &toolCalls,
        const QList<QPair<QString, QString>> &toolResults);

    QJsonObject toJson() const;

private:
    QJsonObject m_baseRequest;
    QJsonArray m_messages;
};

} // namespace QodeAssist::Providers
