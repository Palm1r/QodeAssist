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

#include "RequestConfig.hpp"
#include <QJsonObject>
#include <QObject>

namespace QodeAssist::LLMCore {

class RequestHandlerBase : public QObject
{
    Q_OBJECT

public:
    explicit RequestHandlerBase(QObject *parent = nullptr);
    ~RequestHandlerBase() override;

    virtual void sendLLMRequest(const LLMConfig &config, const QJsonObject &request) = 0;
    virtual bool cancelRequest(const QString &id) = 0;

signals:
    void completionReceived(const QString &completion, const QJsonObject &request, bool isComplete);
    void requestFinished(const QString &requestId, bool success, const QString &errorString);
    void requestCancelled(const QString &id);
};

} // namespace QodeAssist::LLMCore
