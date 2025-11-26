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

