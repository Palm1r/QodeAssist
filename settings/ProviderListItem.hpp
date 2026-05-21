// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFrame>

class QLabel;
class QMouseEvent;

namespace QodeAssist::Providers {
struct ProviderInstance;
}

namespace QodeAssist::Settings {

class ProviderListItem : public QFrame
{
    Q_OBJECT
public:
    enum class Status : int { Unknown, Ok, Fail };

    explicit ProviderListItem(
        const Providers::ProviderInstance &inst, QWidget *parent = nullptr);

    void setStatus(Status s);
    void setSelected(bool s);
    QString providerName() const { return m_name; }

signals:
    void clicked(const QString &name);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    static QString statusColor(Status s);
    void applyTheme();

    QString m_name;
    Status m_status = Status::Unknown;
    bool m_selected = false;
    bool m_inApplyTheme = false;
    QLabel *m_statusDot = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_urlLabel = nullptr;
};

} // namespace QodeAssist::Settings
