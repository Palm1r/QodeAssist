// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QCoreApplication>

#include <utils/differ.h>

namespace QodeAssist {

class DiffStatistics
{
    Q_DECLARE_TR_FUNCTIONS(DiffStatistics)

public:
    DiffStatistics() = default;

    void calculate(const QList<Utils::Diff> &diffList)
    {
        m_linesAdded = 0;
        m_linesRemoved = 0;

        for (const auto &diff : diffList) {
            if (diff.command == Utils::Diff::Insert) {
                m_linesAdded += diff.text.count('\n') + (diff.text.isEmpty() ? 0 : 1);
            } else if (diff.command == Utils::Diff::Delete) {
                m_linesRemoved += diff.text.count('\n') + (diff.text.isEmpty() ? 0 : 1);
            }
        }
    }

    int linesAdded() const { return m_linesAdded; }
    int linesRemoved() const { return m_linesRemoved; }

    QString formatSummary() const
    {
        if (m_linesAdded > 0 && m_linesRemoved > 0) {
            return tr("+%1 lines, -%2 lines").arg(m_linesAdded).arg(m_linesRemoved);
        } else if (m_linesAdded > 0) {
            return tr("+%1 lines").arg(m_linesAdded);
        } else if (m_linesRemoved > 0) {
            return tr("-%1 lines").arg(m_linesRemoved);
        }
        return tr("No changes");
    }

private:
    int m_linesAdded{0};
    int m_linesRemoved{0};
};

} // namespace QodeAssist

