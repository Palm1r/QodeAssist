// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <functional>
#include <QHash>
#include <QObject>
#include <QString>

namespace QodeAssist::TaskFlow {

class Flow;
class FlowManager;

class FlowRegistry : public QObject
{
    Q_OBJECT
public:
    using FlowCreator = std::function<Flow *(FlowManager *flowManager)>;

    explicit FlowRegistry(QObject *parent = nullptr);

    void registerFlow(const QString &flowType, FlowCreator creator);
    Flow *createFlow(const QString &flowType, FlowManager *flowManager = nullptr) const;
    QStringList getAvailableTypes() const;

private:
    QHash<QString, FlowCreator> m_flowCreators;
};

} // namespace QodeAssist::TaskFlow
