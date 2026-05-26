// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFrame>
#include <QString>

class QLabel;

namespace QodeAssist::Settings {

class TagChip : public QFrame
{
    Q_OBJECT
public:
    explicit TagChip(const QString &tag, int count, QWidget *parent = nullptr);

    void setActive(bool on);
    QString tag() const { return m_tag; }

signals:
    void clicked(const QString &tag);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void applyTheme();

    QString m_tag;
    bool m_active = false;
    bool m_inApplyTheme = false;
    QLabel *m_label = nullptr;
    QLabel *m_count = nullptr;
};

} // namespace QodeAssist::Settings
