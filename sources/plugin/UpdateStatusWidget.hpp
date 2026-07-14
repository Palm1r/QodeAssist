// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QToolButton>

QT_BEGIN_NAMESPACE
class QMenu;
QT_END_NAMESPACE

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
    void setChatButtonMenu(QMenu *menu);

    QPushButton *updateButton() const;

private:
    QToolButton *m_actionButton;
    QToolButton *m_chatButton;
    QLabel *m_versionLabel;
    QPushButton *m_updateButton;
};
} // namespace QodeAssist
