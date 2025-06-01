/*
 * Copyright (C) 2025 Petr Mironychev
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

#include "BaseTask.hpp"
#include "llmcore/RequestHandler.hpp"
#include <QFuture>
#include <QJsonObject>
#include <QPromise>

namespace QodeAssist {

class SimpleLLMTask : public BaseTask
{
    Q_OBJECT

public:
    explicit SimpleLLMTask(QObject *parent = nullptr);
    ~SimpleLLMTask() override;

protected:
    TaskState execute() override;

private slots:
    void onLLMResponseReceived(const QString &response, const QJsonObject &request, bool isComplete);
    void onLLMRequestFinished(const QString &requestId, bool success, const QString &errorString);

private:
    bool sendLLMRequest(const QString &actualPrompt);
    void completeTask(TaskState state);

private:
    QString m_currentRequestId;
    QString m_accumulatedResponse;
    LLMCore::RequestHandler *m_requestHandler;

    QPromise<TaskState> *m_promise;
};

} // namespace QodeAssist
