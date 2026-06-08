// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QFrame>
#include <QList>
#include <QSet>
#include <QString>

#include <AgentConfig.hpp>

namespace QodeAssist::Settings {

class TagChip;

class AgentListItem : public QFrame
{
    Q_OBJECT
public:
    explicit AgentListItem(const AgentConfig &cfg, QWidget *parent = nullptr);

    QString agentName() const { return m_name; }
    void setSelected(bool selected);
    void setActiveTags(const QSet<QString> &active);

signals:
    void clicked(const QString &name);
    void tagClicked(const QString &tag);

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void applyTheme();

    QString m_name;
    bool m_selected = false;
    bool m_inApplyTheme = false;
    QList<TagChip *> m_chips;
};

} // namespace QodeAssist::Settings
