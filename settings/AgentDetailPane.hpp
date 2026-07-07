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
class QFrame;
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

class ModelSelector;
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
    void fillRawToml(QPlainTextEdit *view, const QString &path);
    void setDetailsVisible(bool visible);
    void applyCodePalette();
    void populateProviderCombo();
    void onModelSubmitted(const QString &model);
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

    QPointer<Providers::ProviderInstanceFactory> m_instanceFactory;
    QPointer<AgentFactory> m_agentFactory;

    QLabel *m_name = nullptr;
    QLabel *m_sourceBadge = nullptr;
    QLabel *m_path = nullptr;
    QFrame *m_headerSep = nullptr;
    QWidget *m_actionsHolder = nullptr;
    QWidget *m_body = nullptr;
    QPushButton *m_openBtn = nullptr;
    QPushButton *m_dupBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
    QLabel *m_description = nullptr;

    ModelSelector *m_modelSelector = nullptr;
    QPushButton *m_modelResetBtn = nullptr;
    QLabel *m_modelStatus = nullptr;
    QComboBox *m_providerCombo = nullptr;
    QPushButton *m_providerResetBtn = nullptr;
    QCheckBox *m_toolsCheck = nullptr;
    QPushButton *m_toolsResetBtn = nullptr;
    QLabel *m_thinkingValue = nullptr;

    QToolButton *m_detailsToggle = nullptr;
    QWidget *m_detailsBody = nullptr;
    QLineEdit *m_extendsValue = nullptr;
    QLineEdit *m_tagsValue = nullptr;
    QLineEdit *m_endpointValue = nullptr;
    QLabel *m_effectiveUrl = nullptr;
    QLineEdit *m_filePatternsValue = nullptr;

    QToolButton *m_rawToggle = nullptr;
    QPlainTextEdit *m_rawToml = nullptr;
    QToolButton *m_baseRawToggle = nullptr;
    QPlainTextEdit *m_baseRawToml = nullptr;
};

} // namespace QodeAssist::Settings
