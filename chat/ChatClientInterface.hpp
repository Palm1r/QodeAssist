/* 
 * Copyright (C) 2024 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QObject>
#include <QtCore/qjsonarray.h>
#include "QodeAssistData.hpp"
#include "core/LLMRequestHandler.hpp"

namespace QodeAssist::Chat {

class ChatClientInterface : public QObject
{
    Q_OBJECT

public:
    explicit ChatClientInterface(QObject *parent = nullptr);
    ~ChatClientInterface();

    void sendMessage(const QString &message);

signals:
    void messageReceived(const QString &message);
    void errorOccurred(const QString &error);

private:
    void handleLLMResponse(const QString &response, bool isComplete);
    void trimChatHistory();

    LLMRequestHandler *m_requestHandler;
    QString m_accumulatedResponse;
    QString m_pendingMessage;
    QJsonArray m_chatHistory;
};

} // namespace QodeAssist::Chat
