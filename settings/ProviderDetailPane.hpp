// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QWidget>

#include "ProviderInstance.hpp"
#include "ProviderLauncher.hpp"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QToolButton;

namespace TerminalSolution {
class TerminalView;
}

namespace QodeAssist::Settings {

class SectionBox;

class ProviderDetailPane : public QWidget
{
    Q_OBJECT
public:
    explicit ProviderDetailPane(QWidget *parent = nullptr);

    void populate(const Providers::ProviderInstance &inst, bool hasStoredKey);
    void clear();
    void refreshKeyStatus(bool hasStoredKey);
    void setLaunchState(Providers::ProviderLauncher::State st, const QString &lastError);
    void resetLaunchTerminal(const QByteArray &scrollback);
    void appendLaunchBytes(const QByteArray &chunk);

    QString currentName() const { return m_current.name; }

signals:
    void saveRequested(const Providers::ProviderInstance &edited);
    void duplicateRequested();
    void deleteRequested();
    void apiKeySaveRequested(const QString &newKey);
    void apiKeyClearRequested();
    void launchStartRequested(const QString &providerName);
    void launchStopRequested(const QString &providerName);
    void launchRestartRequested(const QString &providerName);
    void openInEditorRequested(const QString &sourcePath);

protected:
    void changeEvent(QEvent *event) override;

private:
    void setEditing(bool on);
    Providers::ProviderInstance collectEdits() const;
    void applyPreviewPalette();
    void applyTerminalPalette();

    bool m_editing = false;
    bool m_currentHasStoredKey = false;
    Providers::ProviderInstance m_current;

    QLabel *m_nameLabel = nullptr;
    QLabel *m_sourcePathLabel = nullptr;
    QPushButton *m_editBtn = nullptr;
    QPushButton *m_openInEditorBtn = nullptr;
    QPushButton *m_dupBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;

    QLabel *m_descriptionLabel = nullptr;

    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_typeEdit = nullptr;
    QPlainTextEdit *m_descriptionEdit = nullptr;
    QLineEdit *m_urlEdit = nullptr;
    QLabel *m_samplePreview = nullptr;

    QLineEdit *m_apiKeyEdit = nullptr;
    QToolButton *m_revealKeyBtn = nullptr;
    QLabel *m_keyHint = nullptr;
    QPushButton *m_apiKeySaveBtn = nullptr;
    QPushButton *m_apiKeyClearBtn = nullptr;
    QPushButton *m_legacyKeyBtn = nullptr;
    QString m_legacyKeyValue;

    SectionBox *m_launchSection = nullptr;
    QLabel *m_launchEmptyHint = nullptr;
    QLabel *m_launchCmdLabel = nullptr;
    QLabel *m_launchStatusPill = nullptr;
    QPushButton *m_startBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_restartBtn = nullptr;
    QToolButton *m_launchTerminalToggle = nullptr;
    TerminalSolution::TerminalView *m_launchTerminal = nullptr;

    QToolButton *m_rawToggle = nullptr;
    QPlainTextEdit *m_rawToml = nullptr;
};

} // namespace QodeAssist::Settings
