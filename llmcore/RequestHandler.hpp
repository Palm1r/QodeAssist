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

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>

#include "RequestConfig.hpp"

class QNetworkReply;

namespace QodeAssist::LLMCore {

class RequestHandler : public QObject
{
    Q_OBJECT

public:
    explicit RequestHandler(QObject *parent = nullptr);

    void sendLLMRequest(const LLMConfig &config, const QJsonObject &request);
    void handleLLMResponse(QNetworkReply *reply,
                           const QJsonObject &request,
                           const LLMConfig &config);
    bool cancelRequest(const QString &id);

signals:
    void completionReceived(const QString &completion, const QJsonObject &request, bool isComplete);
    void requestFinished(const QString &requestId, bool success, const QString &errorString);
    void requestCancelled(const QString &id);

private:
    QNetworkAccessManager *m_manager;
    QMap<QString, QNetworkReply *> m_activeRequests;
    QMap<QNetworkReply *, QString> m_accumulatedResponses;

    bool processSingleLineCompletion(QNetworkReply *reply,
                                     const QJsonObject &request,
                                     const QString &accumulatedResponse,
                                     const LLMConfig &config);
    QString removeStopWords(const QStringView &completion, const QStringList &stopWords);
    void removeCodeBlockWrappers(QString &response);
};

} // namespace QodeAssist::LLMCore
