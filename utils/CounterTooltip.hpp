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

#include <QLabel>
#include <QTimer>
#include <QToolBar>
#include <QWidget>

namespace QodeAssist {

class CounterTooltip : public QToolBar
{
    Q_OBJECT

public:
    CounterTooltip(int count);
    ~CounterTooltip();

signals:
    void finished(int count);

private:
    void updateLabel();

    QLabel *m_label;
    QTimer *m_timer;
    int m_count;
};

} // namespace QodeAssist
