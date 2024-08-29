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

#include <languageclient/languageclientinterface.h>
#include <texteditor/texteditor.h>

class QNetworkReply;
class QNetworkAccessManager;

namespace QodeAssist {

class LLMClientInterface : public LanguageClient::BaseClientInterface
{
    Q_OBJECT

public:
    LLMClientInterface();

public:
    struct ContextPair
    {
        QString prefix;
        QString suffix;
    };

    Utils::FilePath serverDeviceTemplate() const override;

    void sendCompletionToClient(const QString &completion,
                                const QJsonObject &request,
                                const QJsonObject &position,
                                bool isComplete);

    void handleCompletion(const QJsonObject &request,
                          const QString &accumulatedCompletion = QString());
    void sendLLMRequest(const QJsonObject &request, const ContextPair &prompt);
    void handleLLMResponse(QNetworkReply *reply, const QJsonObject &request);

    ContextPair prepareContext(const QJsonObject &request,
                               const QString &accumulatedCompletion = QString{});
    void updateProvider();

protected:
    void startImpl() override;
    void sendData(const QByteArray &data) override;
    void parseCurrentMessage() override;

private:
    void handleInitialize(const QJsonObject &request);
    void handleShutdown(const QJsonObject &request);
    void handleTextDocumentDidOpen(const QJsonObject &request);
    void handleInitialized(const QJsonObject &request);
    void handleExit(const QJsonObject &request);
    void handleCancelRequest(const QJsonObject &request);
    bool processSingleLineCompletion(QNetworkReply *reply,
                                     const QJsonObject &request,
                                     const QString &accumulatedCompletion);

    QString сontextBefore(TextEditor::TextEditorWidget *widget, int lineNumber, int cursorPosition);
    QString сontextAfter(TextEditor::TextEditorWidget *widget, int lineNumber, int cursorPosition);
    QString removeStopWords(const QString &completion);

    QUrl m_serverUrl;
    QNetworkAccessManager *m_manager;
    QMap<QString, QNetworkReply *> m_activeRequests;
    QMap<QNetworkReply *, QString> m_accumulatedResponses;
};

} // namespace QodeAssist
