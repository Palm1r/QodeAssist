// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QLabel>
#include <QWidget>

#include <AgentConfig.hpp>

class QPushButton;

namespace QodeAssist::Settings {

class Pill : public QLabel
{
    Q_OBJECT
public:
    enum Kind { Template, On, Off, User, Tag };

    Pill(Kind kind, const QString &text = {}, QWidget *parent = nullptr);
    void setKind(Kind kind);

protected:
    void changeEvent(QEvent *event) override;

private:
    void applyTheme();

    Kind m_kind;
    bool m_inApplyTheme = false;
};

class AgentSlotWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AgentSlotWidget(QWidget *parent = nullptr);

    void setAgentConfig(const AgentConfig &cfg);
    void clear();

    QPushButton *changeButton() const { return m_changeBtn; }

signals:
    void changeRequested();
    void editRequested();

protected:
    void changeEvent(QEvent *event) override;

private:
    void applyTheme();

    QLabel *m_name = nullptr;
    Pill *m_sourcePill = nullptr;
    QPushButton *m_changeBtn = nullptr;
    QPushButton *m_editBtn = nullptr;

    QLabel *m_modelLabel = nullptr;
    QLabel *m_modelValue = nullptr;
    QLabel *m_urlLabel = nullptr;
    QLabel *m_urlValue = nullptr;
    QLabel *m_endpointLabel = nullptr;
    QLabel *m_endpointValue = nullptr;
    QLabel *m_templateLabel = nullptr;
    QLabel *m_templateValue = nullptr;

    QLabel *m_description = nullptr;

    Pill *m_thinkingPill = nullptr;
    Pill *m_toolsPill = nullptr;

    bool m_inApplyTheme = false;
};

} // namespace QodeAssist::Settings
