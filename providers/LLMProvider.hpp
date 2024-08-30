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

#include <QString>
#include <utils/environment.h>

class QNetworkReply;
class QJsonObject;

namespace QodeAssist::Providers {

class LLMProvider
{
public:
    virtual ~LLMProvider() = default;

    virtual QString name() const = 0;
    virtual QString url() const = 0;
    virtual QString completionEndpoint() const = 0;

    virtual void prepareRequest(QJsonObject &request) = 0;
    virtual bool handleResponse(QNetworkReply *reply, QString &accumulatedResponse) = 0;
    virtual QList<QString> getInstalledModels(const Utils::Environment &env) = 0;
};

} // namespace QodeAssist::Providers
