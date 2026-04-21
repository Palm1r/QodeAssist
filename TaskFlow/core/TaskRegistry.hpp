// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <QHash>
#include <QObject>
#include <QString>

namespace QodeAssist::TaskFlow {

class BaseTask;

class TaskRegistry : public QObject
{
    Q_OBJECT
public:
    using TaskCreator = std::function<BaseTask *(QObject *parent)>;

    explicit TaskRegistry(QObject *parent = nullptr);

    template<typename T>
    inline void registerTask(const QString &taskType)
    {
        m_creators[taskType] = [](QObject *parent) -> BaseTask * { return new T(parent); };
    }
    BaseTask *createTask(const QString &taskType, QObject *parent = nullptr) const;
    QStringList getAvailableTypes() const;

private:
    QHash<QString, TaskCreator> m_creators;
};

} // namespace QodeAssist::TaskFlow
