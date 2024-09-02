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

#include "CounterTooltip.hpp"

namespace QodeAssist {

CounterTooltip::CounterTooltip(int count)
    : m_count(count)
{
    m_label = new QLabel(this);
    addWidget(m_label);
    updateLabel();

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(2000);

    connect(m_timer, &QTimer::timeout, this, [this] { emit finished(m_count); });

    m_timer->start();
}

CounterTooltip::~CounterTooltip() {}

void CounterTooltip::updateLabel()
{
    const auto hotkey = QKeySequence(QKeySequence::MoveToNextWord).toString();
    m_label->setText(QString("Insert Next %1 line(s) (%2)").arg(m_count).arg(hotkey));
}

} // namespace QodeAssist
