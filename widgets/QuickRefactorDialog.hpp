/* 
 * Copyright (C) 2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

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

private:
    void setupUi();
    void createActionButtons();
    CustomInstruction findCurrentInstruction() const;

    QLineEdit *m_quickInstructionEdit;
    QPlainTextEdit *m_textEdit;
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
    QLabel *m_instructionsLabel;

    Action m_selectedAction = Action::Custom;
    QString m_lastInstructions;
    QString m_selectedConfiguration;
    
    QIcon m_toolsIconOn;
    QIcon m_toolsIconOff;
    QIcon m_thinkingIconOn;
    QIcon m_thinkingIconOff;
};

} // namespace QodeAssist
