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

#include "PromptTemplate.hpp"

#include <QJsonArray>
#include <QJsonDocument>

#include "QodeAssistSettings.hpp"
#include "QodeAssistUtils.hpp"
#include "settings/CustomPromptSettings.hpp"

namespace QodeAssist::Templates {

class CustomTemplate : public PromptTemplate
{
public:
    QString name() const override { return "Custom Template"; }
    QString promptTemplate() const override
    {
        return Settings::customPromptSettings().customJsonTemplate();
    }
    QStringList stopWords() const override { return QStringList(); }

    void prepareRequest(QJsonObject &request, const ContextData &context) const override
    {
        QJsonDocument doc = QJsonDocument::fromJson(promptTemplate().toUtf8());
        if (doc.isNull() || !doc.isObject()) {
            logMessage(QString("Invalid JSON template in settings"));

            return;
        }

        QJsonObject templateObj = doc.object();
        QJsonObject processedObj = processJsonTemplate(templateObj, context);

        for (auto it = processedObj.begin(); it != processedObj.end(); ++it) {
            request[it.key()] = it.value();
        }
    }

private:
    QJsonValue processJsonValue(const QJsonValue &value, const ContextData &context) const
    {
        if (value.isString()) {
            QString str = value.toString();
            str.replace("{{QODE_INSTRUCTIONS}}", context.instriuctions);
            str.replace("{{QODE_PREFIX}}", context.prefix);
            str.replace("{{QODE_SUFFIX}}", context.suffix);
            return str;
        } else if (value.isObject()) {
            return processJsonTemplate(value.toObject(), context);
        } else if (value.isArray()) {
            QJsonArray newArray;
            for (const QJsonValue &arrayValue : value.toArray()) {
                newArray.append(processJsonValue(arrayValue, context));
            }
            return newArray;
        }
        return value;
    }

    QJsonObject processJsonTemplate(const QJsonObject &templateObj, const ContextData &context) const
    {
        QJsonObject result;
        for (auto it = templateObj.begin(); it != templateObj.end(); ++it) {
            result[it.key()] = processJsonValue(it.value(), context);
        }
        return result;
    }
};
} // namespace QodeAssist::Templates
