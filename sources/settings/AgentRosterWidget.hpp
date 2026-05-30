// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <AgentRouter.hpp>

class QLabel;
class QToolButton;
class QPushButton;
class QVBoxLayout;
class QFrame;

namespace QodeAssist {
class AgentFactory;
}

namespace QodeAssist::Settings {

class AgentRosterRow;

class AgentRosterWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AgentRosterWidget(QWidget *parent = nullptr);

    void setSlot(
        const QString &title,
        const QString &hint,
        const QStringList &presetTags = {});
    void setRoster(const QStringList &names, AgentFactory *factory);

    // When false, the list is an unordered set: no move arrows, no position
    // numbers, no "first matching" routing hint. Used by pipelines where the
    // user — not a router — chooses the agent (e.g. the chat picker).
    void setOrderable(bool orderable);

    // When true, the card holds at most one agent: "Add" becomes "Change",
    // selecting replaces the current entry, and the routing footer is hidden.
    // Implies a non-orderable list. Used by single-agent pipelines.
    void setSingle(bool single);

    [[nodiscard]] QStringList roster() const { return m_names; }

signals:
    void rosterChanged(const QStringList &names);
    void editAgentRequested(const QString &agentName);

private:
    void rebuildRows();
    void recomputeActive();
    void applyMode();

    void onAddClicked();
    void onRowMoveUp(int index);
    void onRowMoveDown(int index);
    void onRowRemove(int index);
    void onRowEdit(int index);

    QStringList m_names;
    QStringList m_presetTags;
    QPointer<AgentFactory> m_factory;
    QMetaObject::Connection m_factoryConn;
    QMetaObject::Connection m_modelConn;
    AgentRouter::Context m_routingCtx;
    int m_activeIndex = -1;
    bool m_orderable = true;
    bool m_single = false;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    QFrame *m_rowsFrame = nullptr;
    QVBoxLayout *m_rowsLayout = nullptr;
    QLabel *m_emptyHint = nullptr;
    QPushButton *m_addBtn = nullptr;
    QLabel *m_footerHint = nullptr;
};

} // namespace QodeAssist::Settings
