// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
