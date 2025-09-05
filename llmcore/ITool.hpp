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

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace QodeAssist::LLMCore {

enum class ToolState { Idle, Executing, Completed, Failed };

class ITool : public QObject
{
    Q_OBJECT

public:
    explicit ITool(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(this, &ITool::toolStarted, this, [this](const QString &) {
            setState(ToolState::Executing);
        });
        connect(this, &ITool::toolCompleted, this, [this](const QString &, const QString &) {
            setState(ToolState::Completed);
        });
        connect(this, &ITool::toolFailed, this, [this](const QString &, const QString &) {
            setState(ToolState::Failed);
        });
    }
    virtual ~ITool() = default;

    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QJsonObject getDefinition() const = 0;
    virtual QString execute(const QJsonObject &input = QJsonObject()) = 0;

    ToolState state() const { return m_state; }

signals:
    void toolStarted(const QString &toolName);
    void toolCompleted(const QString &toolName, const QString &result);
    void toolFailed(const QString &toolName, const QString &error);
    void stateChanged(ToolState newState);

protected:
    void setState(ToolState newState)
    {
        if (m_state == newState)
            return;

        m_state = newState;
        emit stateChanged(newState);
    }

private:
    ToolState m_state = ToolState::Idle;
};

} // namespace QodeAssist::LLMCore
