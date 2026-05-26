// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFrame>
#include <QList>
#include <QSet>
#include <QString>
#include <vector>

#include <AgentConfig.hpp>

class QLineEdit;
class QScrollArea;
class QTimer;
class QVBoxLayout;

namespace QodeAssist {
class AgentFactory;
}

namespace QodeAssist::Settings {

class AgentListItem;
class TagFilterStrip;

class AgentListPane : public QFrame
{
    Q_OBJECT
public:
    explicit AgentListPane(AgentFactory *factory, QWidget *parent = nullptr);

    QString currentName() const { return m_currentName; }
    void selectByName(const QString &name);
    void refresh();

signals:
    void currentAgentChanged(const QString &name);

protected:
    void changeEvent(QEvent *event) override;

private:
    void rebuildList();
    void applyFilterHolderTheme();
    bool matchesFilters(const AgentConfig &a, const QString &lowerFilter) const;
    std::vector<const AgentConfig *> visibleAgents() const;
    void setCurrentNameInternal(const QString &name, bool emitSignal);
    void onRowClicked(const QString &name);

    AgentFactory *m_factory;
    QLineEdit *m_filterEdit = nullptr;
    QTimer *m_filterDebounce = nullptr;
    QWidget *m_filterHolder = nullptr;
    TagFilterStrip *m_tagStrip = nullptr;
    QScrollArea *m_listScroll = nullptr;
    QList<AgentListItem *> m_rows;
    QString m_currentName;
};

} // namespace QodeAssist::Settings
