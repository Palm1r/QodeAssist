// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QFrame>

class QLabel;

namespace QodeAssist::Settings {

class CollapsibleHeader : public QFrame
{
    Q_OBJECT
public:
    explicit CollapsibleHeader(const QString &title, int count, QWidget *parent = nullptr);

    void setExpanded(bool expanded);
    bool isExpanded() const { return m_expanded; }
    void setClickable(bool clickable);

signals:
    void toggled();

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void applyTheme();
    void updateArrow();

    bool m_expanded = false;
    bool m_clickable = true;
    bool m_hovered = false;
    bool m_inApplyTheme = false;
    QLabel *m_arrow = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_count = nullptr;
};

} // namespace QodeAssist::Settings
