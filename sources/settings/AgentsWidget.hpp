// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <optional>

#include <coreplugin/dialogs/ioptionspage.h>
#include <utils/aspects.h>

#include "acp/AgentDefinition.hpp"

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTextBrowser;
class QVBoxLayout;
QT_END_NAMESPACE

namespace QodeAssist::Acp {
class AgentCatalogStore;
class AgentTester;
} // namespace QodeAssist::Acp

namespace QodeAssist::Settings {

class AgentsWidget : public Core::IOptionsPageWidget
{
    Q_OBJECT

public:
    AgentsWidget();

    void apply() override;

private:
    void setupUI();
    void populateList();
    void updateButtons();
    void showSelectedAgent();
    void showStatus(const QString &message);

    void onTest();
    void onRefresh();
    void onOpenAgentsFolder();

    std::optional<Acp::AgentDefinition> selectedAgent() const;
    static QString testWorkingDirectory();

    Acp::AgentCatalogStore *m_store = nullptr;
    Acp::AgentTester *m_tester = nullptr;

    QLineEdit *addSettingRow(QVBoxLayout *layout, Utils::StringAspect &aspect);

    QLineEdit *m_extraPathsEdit = nullptr;
    QLineEdit *m_forwardedVariablesEdit = nullptr;
    QListWidget *m_agentsList = nullptr;
    QTextBrowser *m_details = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_testButton = nullptr;
    QPushButton *m_reloadButton = nullptr;
    QPushButton *m_refreshButton = nullptr;
};

} // namespace QodeAssist::Settings
