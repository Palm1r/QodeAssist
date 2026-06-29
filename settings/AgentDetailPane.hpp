// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QPointer>
#include <QStringList>
#include <QWidget>

#include <AgentConfig.hpp>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QToolButton;

namespace QodeAssist {
class AgentFactory;
}

namespace QodeAssist::Providers {
class ProviderInstanceFactory;
}

namespace QodeAssist::Settings {

class SectionBox;

class AgentDetailPane : public QWidget
{
    Q_OBJECT
public:
    explicit AgentDetailPane(QWidget *parent = nullptr);

    void setInstanceFactory(Providers::ProviderInstanceFactory *factory);
    void setAgentFactory(AgentFactory *factory);
    void setAgent(const AgentConfig &cfg);
    void clear();

signals:
    void openInEditorRequested(const AgentConfig &cfg);
    void customizeRequested(const AgentConfig &cfg);
    void deleteRequested(const AgentConfig &cfg);

protected:
    void changeEvent(QEvent *event) override;

private:
    QLineEdit *makeReadOnlyLine(bool mono = false);
    void applyCodePalette();
    void populateProviderCombo();
    void onChangeModel();
    void onResetModel();
    void onChangeProvider(int index);
    void onResetProvider();
    void onToggleTools(bool enabled);
    void onResetTools();

    bool m_inApplyPalette = false;
    bool m_providerComboPopulated = false;
    bool m_providerComboHasSentinel = false;

    AgentConfig m_currentStorage;
    const AgentConfig *m_current = nullptr;

    QLabel *m_name = nullptr;
    QLabel *m_path = nullptr;
    QPushButton *m_openBtn = nullptr;
    QPushButton *m_dupBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
    QLabel *m_description = nullptr;

    QLineEdit *m_nameValue = nullptr;
    QLabel *m_extendsLabel = nullptr;
    QWidget *m_extendsHolder = nullptr;
    QLineEdit *m_extendsValue = nullptr;
    QPlainTextEdit *m_descriptionEdit = nullptr;
    QLineEdit *m_tagsValue = nullptr;

    QComboBox *m_providerCombo = nullptr;
    QPushButton *m_providerResetBtn = nullptr;
    QPointer<Providers::ProviderInstanceFactory> m_instanceFactory;
    QPointer<AgentFactory> m_agentFactory;
    QLineEdit *m_endpointValue = nullptr;
    QLineEdit *m_modelValue = nullptr;
    QPushButton *m_modelChangeBtn = nullptr;
    QPushButton *m_modelResetBtn = nullptr;
    QLabel *m_effectiveUrl = nullptr;

    QLabel *m_thinkingValue = nullptr;
    QCheckBox *m_toolsCheck = nullptr;
    QPushButton *m_toolsResetBtn = nullptr;

    QLineEdit *m_filePatternsValue = nullptr;

    QToolButton *m_rawToggle = nullptr;
    QPlainTextEdit *m_rawToml = nullptr;
};

} // namespace QodeAssist::Settings
