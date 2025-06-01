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

#include "tasks/Flow.hpp"
#include "tasks/FlowManager.hpp"
#include <QDialog>
#include <QString>
#include <QtWidgets/qapplication.h>

class QPlainTextEdit;
class QToolButton;
class QLabel;

namespace QodeAssist {

class QuickRefactorDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Action { Custom, RepeatLast, ImproveCode, AlternativeSolution };

    explicit QuickRefactorDialog(
        FlowManager *flowManager,
        QWidget *parent = nullptr,
        const QString &lastInstructions = QString());
    ~QuickRefactorDialog() override;

    QString instructions() const;
    void setInstructions(const QString &instructions);

    Action selectedAction() const;

private slots:
    void useLastInstructions();
    void useImproveCodeTemplate();
    void useAlternativeSolutionTemplate();
    void updateDialogSize();
    void onRunTasksClicked();
    void onTextChanged();

private:
    void setupUi();
    void createActionButtons();
    QString getFlowsFilePath() const;

    QPlainTextEdit *m_textEdit;
    QToolButton *m_repeatButton;
    QToolButton *m_improveButton;
    QToolButton *m_alternativeButton;
    QLabel *m_instructionsLabel;

    Action m_selectedAction = Action::Custom;
    QString m_lastInstructions;
    QToolButton *m_runTasksButton;

    FlowManager *m_flowManager;
};

} // namespace QodeAssist
