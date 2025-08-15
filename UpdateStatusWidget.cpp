/* 
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include "UpdateStatusWidget.hpp"

namespace QodeAssist {

UpdateStatusWidget::UpdateStatusWidget(QWidget *parent)
    : QFrame(parent)
{
    setFrameStyle(QFrame::NoFrame);

    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(4);

    m_actionButton = new QToolButton(this);
    m_actionButton->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_chatButton = new QToolButton(this);
    m_chatButton->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_versionLabel = new QLabel(this);
    m_versionLabel->setVisible(false);

    m_updateButton = new QPushButton(tr("Update"), this);
    m_updateButton->setVisible(false);
    m_updateButton->setStyleSheet("QPushButton { padding: 2px 8px; }");

    layout->addWidget(m_actionButton);
    layout->addWidget(m_chatButton);
    layout->addWidget(m_versionLabel);
    layout->addWidget(m_updateButton);
}

void UpdateStatusWidget::setDefaultAction(QAction *action)
{
    m_actionButton->setDefaultAction(action);
}

void UpdateStatusWidget::showUpdateAvailable(const QString &version)
{
    m_versionLabel->setText(tr("New version: v%1").arg(version));
    m_versionLabel->setVisible(true);
    m_updateButton->setVisible(true);
    m_updateButton->setToolTip(tr("Check update information"));
}

void UpdateStatusWidget::hideUpdateInfo()
{
    m_versionLabel->setVisible(false);
    m_updateButton->setVisible(false);
}

void UpdateStatusWidget::setChatButtonAction(QAction *action)
{
    m_chatButton->setDefaultAction(action);
}

QPushButton *UpdateStatusWidget::updateButton() const
{
    return m_updateButton;
}

} // namespace QodeAssist
