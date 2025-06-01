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

#include "QuickRefactorDialog.hpp"
#include "FlowRegistry.hpp"
#include "QodeAssisttr.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "logger/Logger.hpp"
#include "tasks/BaseTask.hpp"
// #include "tasks/DemoTasks.hpp"
// #include "tasks/SimpleLLMTask.hpp"
#include <coreplugin/icore.h>
#include <llmcore/PromptTemplateManager.hpp>
#include <llmcore/ProvidersManager.hpp>
#include <llmcore/RequestHandler.hpp>
#include <settings/ChatAssistantSettings.hpp>
#include <settings/GeneralSettings.hpp>
#include <utils/theme/theme.h>
#include <utils/utilsicons.h>
#include <QFutureWatcher>
#include <QQuickView>
#include <QUuid>

namespace QodeAssist {

QuickRefactorDialog::QuickRefactorDialog(
    FlowManager *flowManager, QWidget *parent, const QString &lastInstructions)
    : QDialog(parent)
    , m_lastInstructions(lastInstructions)
    , m_flowManager(flowManager)
{
    setWindowTitle(Tr::tr("Quick Refactor"));
    setupUi();

    QTimer::singleShot(0, this, &QuickRefactorDialog::updateDialogSize);
    m_textEdit->installEventFilter(this);
    updateDialogSize();
}

QuickRefactorDialog::~QuickRefactorDialog()
{
}

void QuickRefactorDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    QHBoxLayout *actionsLayout = new QHBoxLayout();
    actionsLayout->setSpacing(4);
    createActionButtons();
    actionsLayout->addWidget(m_repeatButton);
    actionsLayout->addWidget(m_improveButton);
    actionsLayout->addWidget(m_alternativeButton);
    actionsLayout->addWidget(m_runTasksButton);
    actionsLayout->addStretch();
    mainLayout->addLayout(actionsLayout);

    m_instructionsLabel = new QLabel(Tr::tr("Enter refactoring instructions:"), this);
    mainLayout->addWidget(m_instructionsLabel);

    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setMinimumHeight(100);
    m_textEdit->setPlaceholderText(Tr::tr("Type your refactoring instructions here..."));

    connect(m_textEdit, &QPlainTextEdit::textChanged, this, &QuickRefactorDialog::onTextChanged);

    mainLayout->addWidget(m_textEdit);

    QDialogButtonBox *buttonBox
        = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void QuickRefactorDialog::createActionButtons()
{
    Utils::Icon REPEAT_ICON(
        {{":/resources/images/repeat-last-instruct-icon.png", Utils::Theme::IconsBaseColor}});
    Utils::Icon IMPROVE_ICON(
        {{":/resources/images/improve-current-code-icon.png", Utils::Theme::IconsBaseColor}});
    Utils::Icon ALTER_ICON(
        {{":/resources/images/suggest-new-icon.png", Utils::Theme::IconsBaseColor}});
    m_repeatButton = new QToolButton(this);
    m_repeatButton->setIcon(REPEAT_ICON.icon());
    m_repeatButton->setToolTip(Tr::tr("Repeat Last Instructions"));
    m_repeatButton->setEnabled(!m_lastInstructions.isEmpty());
    connect(m_repeatButton, &QToolButton::clicked, this, &QuickRefactorDialog::useLastInstructions);

    m_improveButton = new QToolButton(this);
    m_improveButton->setIcon(IMPROVE_ICON.icon());
    m_improveButton->setToolTip(Tr::tr("Improve Current Code"));
    connect(
        m_improveButton, &QToolButton::clicked, this, &QuickRefactorDialog::useImproveCodeTemplate);

    m_alternativeButton = new QToolButton(this);
    m_alternativeButton->setIcon(ALTER_ICON.icon());
    m_alternativeButton->setToolTip(Tr::tr("Suggest Alternative Solution"));
    connect(
        m_alternativeButton,
        &QToolButton::clicked,
        this,
        &QuickRefactorDialog::useAlternativeSolutionTemplate);

    m_runTasksButton = new QToolButton(this);
    m_runTasksButton->setText("Run Tasks");
    m_runTasksButton->setToolTip(Tr::tr("Test Task System"));
    connect(m_runTasksButton, &QToolButton::clicked, this, &QuickRefactorDialog::onRunTasksClicked);
}

QString QuickRefactorDialog::instructions() const
{
    return m_textEdit->toPlainText();
}

void QuickRefactorDialog::setInstructions(const QString &instructions)
{
    m_textEdit->setPlainText(instructions);
}

QuickRefactorDialog::Action QuickRefactorDialog::selectedAction() const
{
    return m_selectedAction;
}

void QuickRefactorDialog::useLastInstructions()
{
    if (!m_lastInstructions.isEmpty()) {
        m_textEdit->setPlainText(m_lastInstructions);
        m_selectedAction = Action::RepeatLast;
    }
    accept();
}

void QuickRefactorDialog::useImproveCodeTemplate()
{
    m_textEdit->setPlainText(Tr::tr(
        "Improve the selected code by enhancing readability, efficiency, and maintainability. "
        "Follow best practices for C++/Qt and fix any potential issues."));
    m_selectedAction = Action::ImproveCode;
    accept();
}

void QuickRefactorDialog::useAlternativeSolutionTemplate()
{
    m_textEdit->setPlainText(
        Tr::tr("Suggest an alternative implementation approach for the selected code. "
               "Provide a different solution that might be cleaner, more efficient, "
               "or uses different Qt/C++ patterns or idioms."));
    m_selectedAction = Action::AlternativeSolution;
    accept();
}

void QuickRefactorDialog::updateDialogSize()
{
    QString text = m_textEdit->toPlainText();

    QFontMetrics fm(m_textEdit->font());

    QStringList lines = text.split('\n');
    int lineCount = lines.size();

    if (lineCount <= 1) {
        int singleLineHeight = fm.height() + 10;
        m_textEdit->setMinimumHeight(singleLineHeight);
        m_textEdit->setMaximumHeight(singleLineHeight);
    } else {
        m_textEdit->setMaximumHeight(QWIDGETSIZE_MAX);

        int lineHeight = fm.height() + 2;

        int textEditHeight = qMin(qMax(lineCount, 2) * lineHeight, 20 * lineHeight);
        m_textEdit->setMinimumHeight(textEditHeight);
    }

    int maxWidth = 500;
    for (const QString &line : lines) {
        int lineWidth = fm.horizontalAdvance(line) + 30;
        maxWidth = qMax(maxWidth, qMin(lineWidth, 800));
    }

    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();

    int newWidth = qMin(maxWidth + 40, screenGeometry.width() * 3 / 4);

    int newHeight;
    if (lineCount <= 1) {
        newHeight = 150;
    } else {
        newHeight = m_textEdit->minimumHeight() + 150;
    }
    newHeight = qMin(newHeight, screenGeometry.height() * 3 / 4);

    resize(newWidth, newHeight);
}

QString QuickRefactorDialog::getFlowsFilePath() const
{
    QString configDir
        = QString("%1/qodeassist/flows").arg(Core::ICore::userResourcePath().toFSPathString());
    QDir().mkpath(configDir);
    return QDir(configDir).filePath("flows.json");
}

void QuickRefactorDialog::onRunTasksClicked()
{
    LOG_MESSAGE("Executing flow using FlowManager...");

    Flow *demoFlow = m_flowManager->flowRegistry()->createFlow("TestFileFlow", m_flowManager);

    LOG_MESSAGE("Starting demo flow execution...");

    QFuture<FlowState> future = demoFlow->executeAsync();

    // Можно использовать watcher для асинхронного выполнения
    QFutureWatcher<FlowState> *watcher = new QFutureWatcher<FlowState>(this);
    connect(watcher, &QFutureWatcher<FlowState>::finished, [this, watcher]() {
        FlowState result = watcher->result();
        LOG_MESSAGE(
            QString("Demo flow finished with state: %1").arg(Flow::flowStateAsString(result)));
        watcher->deleteLater();
    });
    watcher->setFuture(future);

    LOG_MESSAGE("Flow execution started asynchronously from FlowManager...");
}

void QuickRefactorDialog::onTextChanged()
{
    updateDialogSize();
}

} // namespace QodeAssist
