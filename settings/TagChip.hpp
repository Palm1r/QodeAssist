// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QFrame>
#include <QString>

class QLabel;
class QEnterEvent;

namespace QodeAssist::Settings {

class TagChip : public QFrame
{
    Q_OBJECT
public:
    explicit TagChip(const QString &tag, int count, QWidget *parent = nullptr);

    void setActive(bool on);
    void setCount(int count);
    QString tag() const { return m_tag; }

signals:
    void clicked(const QString &tag);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void applyTheme();

    QString m_tag;
    bool m_active = false;
    bool m_hover = false;
    bool m_inApplyTheme = false;
    QLabel *m_label = nullptr;
    QLabel *m_count = nullptr;
};

} // namespace QodeAssist::Settings
