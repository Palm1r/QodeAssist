// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>

namespace QodeAssist {

class AgentKnowledgeServerTest final : public QObject
{
    Q_OBJECT

private slots:
    void testKnowledgeServerExposesOnlyReadOnlyTools();
    void testKnowledgeServerBindsLoopbackOnAnEphemeralPort();
};

} // namespace QodeAssist
