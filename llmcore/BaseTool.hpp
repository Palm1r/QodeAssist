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

#include <QFuture>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace QodeAssist::LLMCore {

enum class ToolSchemaFormat { OpenAI, Claude, Ollama };

class BaseTool : public QObject
{
    Q_OBJECT
public:
    explicit BaseTool(QObject *parent = nullptr);
    ~BaseTool() override = default;

    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QJsonObject getDefinition(ToolSchemaFormat format) const;

    virtual QFuture<QString> executeAsync(const QJsonObject &input = QJsonObject()) = 0;

protected:
    virtual QJsonObject customizeForOpenAI(const QJsonObject &baseDefinition) const;
    virtual QJsonObject customizeForClaude(const QJsonObject &baseDefinition) const;
    virtual QJsonObject customizeForOllama(const QJsonObject &baseDefinition) const;
};

} // namespace QodeAssist::LLMCore
