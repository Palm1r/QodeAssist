// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QString>
#include "CustomInstructionsManager.hpp"

class QPlainTextEdit;
class QToolButton;
class QLabel;
class QComboBox;
class QLineEdit;
class QFrame;

namespace QodeAssist {

class QuickRefactorDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Action { Custom, RepeatLast, ImproveCode, AlternativeSolution };

    explicit QuickRefactorDialog(
        QWidget *parent = nullptr, const QString &lastInstructions = QString());
    ~QuickRefactorDialog() override = default;

    QString instructions() const;
    void setInstructions(const QString &instructions);

    Action selectedAction() const;

    QString selectedConfiguration() const;

    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void useLastInstructions();
    void useImproveCodeTemplate();
    void useAlternativeSolutionTemplate();
    void updateDialogSize();
    void onCommandSelected(int index);
    void onAddCustomCommand();
    void onEditCustomCommand();
    void onDeleteCustomCommand();
    void onOpenInstructionsFolder();
    void onOpenSettings();
    void loadCustomCommands();
    void loadAvailableConfigurations();
    void onConfigurationChanged(int index);
    void validateAndAccept();

private:
    void setupUi();
    void createActionButtons();
    CustomInstruction findCurrentInstruction() const;

    QPlainTextEdit *m_instructionEdit;
    QToolButton *m_repeatButton;
    QToolButton *m_improveButton;
    QToolButton *m_alternativeButton;
    QToolButton *m_addCommandButton;
    QToolButton *m_editCommandButton;
    QToolButton *m_deleteCommandButton;
    QToolButton *m_openFolderButton;
    QToolButton *m_settingsButton;
    QToolButton *m_toolsButton;
    QToolButton *m_thinkingButton;
    QComboBox *m_commandsComboBox;
    QComboBox *m_configComboBox;

    Action m_selectedAction = Action::Custom;
    QString m_lastInstructions;
    QString m_selectedConfiguration;
    
    QIcon m_toolsIconOn;
    QIcon m_toolsIconOff;
    QIcon m_thinkingIconOn;
    QIcon m_thinkingIconOff;
};

} // namespace QodeAssist
