// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QFrame>
#include <QString>

#include <AgentConfig.hpp>

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

namespace Utils {
class ElidingLabel;
}

namespace QodeAssist::Settings {

class AgentListItem : public QFrame
{
    Q_OBJECT
public:
    explicit AgentListItem(const AgentConfig &cfg, QWidget *parent = nullptr);

    QString agentName() const { return m_name; }
    void setSelected(bool selected);
    void setModel(const QString &model);
    void setFilterHighlight(const QString &lowerFilter);

signals:
    void clicked(const QString &name);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void applyTheme();
    void updateNameText();

    QString m_name;
    QString m_model;
    QString m_filter;
    bool m_selected = false;
    bool m_inApplyTheme = false;
    QLabel *m_nameLabel = nullptr;
    Utils::ElidingLabel *m_modelLabel = nullptr;
};

} // namespace QodeAssist::Settings
