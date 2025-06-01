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

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include "Flow.hpp"

namespace QodeAssist {

class FlowManager : public QObject
{
    Q_OBJECT

public:
    explicit FlowManager(QObject *parent = nullptr);
    ~FlowManager() override;

    void addFlow(Flow *flow);
    void removeFlow(const QString &flowId);
    Flow *getFlow(const QString &flowId) const;
    QList<Flow *> getAllFlows() const;
    QStringList getFlowIds() const;

    bool hasFlow(const QString &flowId) const;
    void clear();

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &json);

    bool saveToFile(const QString &filePath) const;
    bool loadFromFile(const QString &filePath);

private:
    QHash<QString, Flow *> m_flows;
};

} // namespace QodeAssist
