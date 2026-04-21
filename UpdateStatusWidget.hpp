// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QToolButton>

namespace QodeAssist {

class UpdateStatusWidget : public QFrame
{
    Q_OBJECT
public:
    explicit UpdateStatusWidget(QWidget *parent = nullptr);

    void setDefaultAction(QAction *action);
    void showUpdateAvailable(const QString &version);
    void hideUpdateInfo();
    void setChatButtonAction(QAction *action);

    QPushButton *updateButton() const;

private:
    QToolButton *m_actionButton;
    QToolButton *m_chatButton;
    QLabel *m_versionLabel;
    QPushButton *m_updateButton;
};
} // namespace QodeAssist
