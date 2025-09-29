/*
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include "ChatRootView.hpp"

#include <QClipboard>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/icore.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectmanager.h>
#include <utils/theme/theme.h>
#include <utils/utilsicons.h>

#include "ChatAssistantSettings.hpp"
#include "ChatSerializer.hpp"
#include "GeneralSettings.hpp"
#include "Logger.hpp"
#include "ProjectSettings.hpp"
#include "context/ContextManager.hpp"
#include "context/TokenUtils.hpp"

namespace QodeAssist::Chat {

ChatRootView::ChatRootView(QQuickItem *parent)
    : QQuickItem(parent)
    , m_chatModel(new ChatModel(this))
    , m_promptProvider(LLMCore::PromptTemplateManager::instance())
    , m_clientInterface(new ClientInterface(m_chatModel, &m_promptProvider, this))
    , m_isRequestInProgress(false)
{
    m_isSyncOpenFiles = Settings::chatAssistantSettings().linkOpenFiles();
    connect(
        &Settings::chatAssistantSettings().linkOpenFiles,
        &Utils::BaseAspect::changed,
        this,
        [this]() { setIsSyncOpenFiles(Settings::chatAssistantSettings().linkOpenFiles()); });

    auto &settings = Settings::generalSettings();

    connect(
        &settings.caModel, &Utils::BaseAspect::changed, this, &ChatRootView::currentTemplateChanged);

    connect(
        m_clientInterface,
        &ClientInterface::messageReceivedCompletely,
        this,
        &ChatRootView::autosave);

    connect(m_clientInterface, &ClientInterface::messageReceivedCompletely, this, [this]() {
        this->setRequestProgressStatus(false);
    });

    connect(
        m_clientInterface,
        &ClientInterface::messageReceivedCompletely,
        this,
        &ChatRootView::updateInputTokensCount);

    connect(m_chatModel, &ChatModel::modelReseted, this, [this]() { setRecentFilePath(QString{}); });
    connect(this, &ChatRootView::attachmentFilesChanged, &ChatRootView::updateInputTokensCount);
    connect(this, &ChatRootView::linkedFilesChanged, &ChatRootView::updateInputTokensCount);
    connect(
        &Settings::chatAssistantSettings().useSystemPrompt,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::updateInputTokensCount);
    connect(
        &Settings::chatAssistantSettings().systemPrompt,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::updateInputTokensCount);

    auto editors = Core::EditorManager::instance();

    connect(editors, &Core::EditorManager::editorCreated, this, &ChatRootView::onEditorCreated);
    connect(
        editors,
        &Core::EditorManager::editorAboutToClose,
        this,
        &ChatRootView::onEditorAboutToClose);

    connect(editors, &Core::EditorManager::currentEditorAboutToChange, this, [this]() {
        if (m_isSyncOpenFiles) {
            for (auto editor : std::as_const(m_currentEditors)) {
                onAppendLinkFileFromEditor(editor);
            }
        }
    });
    connect(
        &Settings::chatAssistantSettings().textFontFamily,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::textFamilyChanged);
    connect(
        &Settings::chatAssistantSettings().codeFontFamily,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::codeFamilyChanged);
    connect(
        &Settings::chatAssistantSettings().textFontSize,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::textFontSizeChanged);
    connect(
        &Settings::chatAssistantSettings().codeFontSize,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::codeFontSizeChanged);
    connect(
        &Settings::chatAssistantSettings().textFormat,
        &Utils::BaseAspect::changed,
        this,
        &ChatRootView::textFormatChanged);

    updateInputTokensCount();
}

ChatModel *ChatRootView::chatModel() const
{
    return m_chatModel;
}

void ChatRootView::sendMessage(const QString &message)
{
    if (m_inputTokensCount > m_chatModel->tokensThreshold()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            Core::ICore::dialogParent(),
            tr("Token Limit Exceeded"),
            tr("The chat history has exceeded the token limit.\n"
               "Would you like to create new chat?"),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            autosave();
            m_chatModel->clear();
            setRecentFilePath(QString{});
            return;
        }
    }

    m_clientInterface->sendMessage(message, m_attachmentFiles, m_linkedFiles);
    clearAttachmentFiles();
    setRequestProgressStatus(true);
}

void ChatRootView::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

void ChatRootView::cancelRequest()
{
    m_clientInterface->cancelRequest();
    setRequestProgressStatus(false);
}

void ChatRootView::clearAttachmentFiles()
{
    if (!m_attachmentFiles.isEmpty()) {
        m_attachmentFiles.clear();
        emit attachmentFilesChanged();
    }
}

void ChatRootView::clearLinkedFiles()
{
    if (!m_linkedFiles.isEmpty()) {
        m_linkedFiles.clear();
        emit linkedFilesChanged();
    }
}

QString ChatRootView::getChatsHistoryDir() const
{
    QString path;

    if (auto project = ProjectExplorer::ProjectManager::startupProject()) {
        Settings::ProjectSettings projectSettings(project);
        path = projectSettings.chatHistoryPath().toFSPathString();
    } else {
        path = QString("%1/qodeassist/chat_history")
                   .arg(Core::ICore::userResourcePath().toFSPathString());
    }

    QDir dir(path);
    if (!dir.exists() && !dir.mkpath(".")) {
        LOG_MESSAGE(QString("Failed to create directory: %1").arg(path));
        return QString();
    }

    return path;
}

QString ChatRootView::currentTemplate() const
{
    auto &settings = Settings::generalSettings();
    return settings.caModel();
}

void ChatRootView::saveHistory(const QString &filePath)
{
    auto result = ChatSerializer::saveToFile(m_chatModel, filePath);
    if (!result.success) {
        LOG_MESSAGE(QString("Failed to save chat history: %1").arg(result.errorMessage));
    } else {
        setRecentFilePath(filePath);
    }
}

void ChatRootView::loadHistory(const QString &filePath)
{
    auto result = ChatSerializer::loadFromFile(m_chatModel, filePath);
    if (!result.success) {
        LOG_MESSAGE(QString("Failed to load chat history: %1").arg(result.errorMessage));
    } else {
        setRecentFilePath(filePath);
    }
    updateInputTokensCount();
}

void ChatRootView::showSaveDialog()
{
    QString initialDir = getChatsHistoryDir();

    QFileDialog *dialog = new QFileDialog(nullptr, tr("Save Chat History"));
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setNameFilter(tr("JSON files (*.json)"));
    dialog->setDefaultSuffix("json");
    if (!initialDir.isEmpty()) {
        dialog->setDirectory(initialDir);
        dialog->selectFile(getSuggestedFileName() + ".json");
    }

    connect(dialog, &QFileDialog::finished, this, [this, dialog](int result) {
        if (result == QFileDialog::Accepted) {
            QStringList files = dialog->selectedFiles();
            if (!files.isEmpty()) {
                saveHistory(files.first());
            }
        }
        dialog->deleteLater();
    });

    dialog->open();
}

void ChatRootView::showLoadDialog()
{
    QString initialDir = getChatsHistoryDir();

    QFileDialog *dialog = new QFileDialog(nullptr, tr("Load Chat History"));
    dialog->setAcceptMode(QFileDialog::AcceptOpen);
    dialog->setFileMode(QFileDialog::ExistingFile);
    dialog->setNameFilter(tr("JSON files (*.json)"));
    if (!initialDir.isEmpty()) {
        dialog->setDirectory(initialDir);
    }

    connect(dialog, &QFileDialog::finished, this, [this, dialog](int result) {
        if (result == QFileDialog::Accepted) {
            QStringList files = dialog->selectedFiles();
            if (!files.isEmpty()) {
                loadHistory(files.first());
            }
        }
        dialog->deleteLater();
    });

    dialog->open();
}

QString ChatRootView::getSuggestedFileName() const
{
    QStringList parts;

    static const QRegularExpression saitizeSymbols = QRegularExpression("[\\/:*?\"<>|\\s]");
    static const QRegularExpression underSymbols = QRegularExpression("_+");

    if (m_chatModel->rowCount() > 0) {
        QString firstMessage
            = m_chatModel->data(m_chatModel->index(0), ChatModel::Content).toString();
        QString shortMessage = firstMessage.split('\n').first().simplified().left(30);

        QString sanitizedMessage = shortMessage;
        sanitizedMessage.replace(saitizeSymbols, "_");
        sanitizedMessage.replace(underSymbols, "_");
        sanitizedMessage = sanitizedMessage.trimmed();

        if (!sanitizedMessage.isEmpty()) {
            if (sanitizedMessage.startsWith('_')) {
                sanitizedMessage.remove(0, 1);
            }
            if (sanitizedMessage.endsWith('_')) {
                sanitizedMessage.chop(1);
            }

            QString targetDir = getChatsHistoryDir();
            QString fullPath = QDir(targetDir).filePath(sanitizedMessage);

            QFileInfo fileInfo(fullPath);
            if (!fileInfo.exists() && QFileInfo(fileInfo.path()).isWritable()) {
                parts << sanitizedMessage;
            }
        }
    }

    parts << QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm");

    QString fileName = parts.join("_");

    QString fullPath = QDir(getChatsHistoryDir()).filePath(fileName);
    QFileInfo finalCheck(fullPath);

    if (fileName.isEmpty() || finalCheck.exists() || !QFileInfo(finalCheck.path()).isWritable()) {
        fileName = QString("chat_%1").arg(
            QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm"));
    }

    return fileName;
}

void ChatRootView::autosave()
{
    if (m_chatModel->rowCount() == 0 || !Settings::chatAssistantSettings().autosave()) {
        return;
    }

    QString filePath = getAutosaveFilePath();
    if (!filePath.isEmpty()) {
        ChatSerializer::saveToFile(m_chatModel, filePath);
        setRecentFilePath(filePath);
    }
}

QString ChatRootView::getAutosaveFilePath() const
{
    if (!m_recentFilePath.isEmpty()) {
        return m_recentFilePath;
    }

    QString dir = getChatsHistoryDir();
    if (dir.isEmpty()) {
        return QString();
    }

    return QDir(dir).filePath(getSuggestedFileName() + ".json");
}

QStringList ChatRootView::attachmentFiles() const
{
    return m_attachmentFiles;
}

QStringList ChatRootView::linkedFiles() const
{
    return m_linkedFiles;
}

void ChatRootView::showAttachFilesDialog()
{
    QFileDialog dialog(nullptr, tr("Select Files to Attach"));
    dialog.setFileMode(QFileDialog::ExistingFiles);

    if (auto project = ProjectExplorer::ProjectManager::startupProject()) {
        dialog.setDirectory(project->projectDirectory().toFSPathString());
    }

    if (dialog.exec() == QDialog::Accepted) {
        QStringList newFilePaths = dialog.selectedFiles();
        if (!newFilePaths.isEmpty()) {
            bool filesAdded = false;
            for (const QString &filePath : std::as_const(newFilePaths)) {
                if (!m_attachmentFiles.contains(filePath)) {
                    m_attachmentFiles.append(filePath);
                    filesAdded = true;
                }
            }
            if (filesAdded) {
                emit attachmentFilesChanged();
            }
        }
    }
}

void ChatRootView::removeFileFromAttachList(int index)
{
    if (index >= 0 && index < m_attachmentFiles.size()) {
        m_attachmentFiles.removeAt(index);
        emit attachmentFilesChanged();
    }
}

void ChatRootView::showLinkFilesDialog()
{
    QFileDialog dialog(nullptr, tr("Select Files to Attach"));
    dialog.setFileMode(QFileDialog::ExistingFiles);

    if (auto project = ProjectExplorer::ProjectManager::startupProject()) {
        dialog.setDirectory(project->projectDirectory().toFSPathString());
    }

    if (dialog.exec() == QDialog::Accepted) {
        QStringList newFilePaths = dialog.selectedFiles();
        if (!newFilePaths.isEmpty()) {
            bool filesAdded = false;
            for (const QString &filePath : std::as_const(newFilePaths)) {
                if (!m_linkedFiles.contains(filePath)) {
                    m_linkedFiles.append(filePath);
                    filesAdded = true;
                }
            }
            if (filesAdded) {
                emit linkedFilesChanged();
            }
        }
    }
}

void ChatRootView::removeFileFromLinkList(int index)
{
    if (index >= 0 && index < m_linkedFiles.size()) {
        m_linkedFiles.removeAt(index);
        emit linkedFilesChanged();
    }
}

void ChatRootView::calculateMessageTokensCount(const QString &message)
{
    m_messageTokensCount = Context::TokenUtils::estimateTokens(message);
    updateInputTokensCount();
}

void ChatRootView::setIsSyncOpenFiles(bool state)
{
    if (m_isSyncOpenFiles != state) {
        m_isSyncOpenFiles = state;
        emit isSyncOpenFilesChanged();
    }

    if (m_isSyncOpenFiles) {
        for (auto editor : std::as_const(m_currentEditors)) {
            onAppendLinkFileFromEditor(editor);
        }
    }
}

void ChatRootView::openChatHistoryFolder()
{
    QString path;
    if (auto project = ProjectExplorer::ProjectManager::startupProject()) {
        Settings::ProjectSettings projectSettings(project);
        path = projectSettings.chatHistoryPath().toFSPathString();
    } else {
        path = QString("%1/qodeassist/chat_history")
                   .arg(Core::ICore::userResourcePath().toFSPathString());
    }

    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QUrl url = QUrl::fromLocalFile(dir.absolutePath());
    QDesktopServices::openUrl(url);
}

void ChatRootView::updateInputTokensCount()
{
    int inputTokens = m_messageTokensCount;
    auto &settings = Settings::chatAssistantSettings();

    if (settings.useSystemPrompt()) {
        inputTokens += Context::TokenUtils::estimateTokens(settings.systemPrompt());
    }

    if (!m_attachmentFiles.isEmpty()) {
        auto attachFiles = m_clientInterface->contextManager()->getContentFiles(m_attachmentFiles);
        inputTokens += Context::TokenUtils::estimateFilesTokens(attachFiles);
    }

    if (!m_linkedFiles.isEmpty()) {
        auto linkFiles = m_clientInterface->contextManager()->getContentFiles(m_linkedFiles);
        inputTokens += Context::TokenUtils::estimateFilesTokens(linkFiles);
    }

    const auto &history = m_chatModel->getChatHistory();
    for (const auto &message : history) {
        inputTokens += Context::TokenUtils::estimateTokens(message.content);
        inputTokens += 4; // + role
    }

    m_inputTokensCount = inputTokens;
    emit inputTokensCountChanged();
}

int ChatRootView::inputTokensCount() const
{
    return m_inputTokensCount;
}

bool ChatRootView::isSyncOpenFiles() const
{
    return m_isSyncOpenFiles;
}

void ChatRootView::onEditorAboutToClose(Core::IEditor *editor)
{
    if (auto document = editor->document(); document && isSyncOpenFiles()) {
        QString filePath = document->filePath().toFSPathString();
        m_linkedFiles.removeOne(filePath);
        emit linkedFilesChanged();
    }

    if (editor) {
        m_currentEditors.removeOne(editor);
    }
}

void ChatRootView::onAppendLinkFileFromEditor(Core::IEditor *editor)
{
    if (auto document = editor->document(); document && isSyncOpenFiles()) {
        QString filePath = document->filePath().toFSPathString();
        if (!m_linkedFiles.contains(filePath) && !shouldIgnoreFileForAttach(document->filePath())) {
            m_linkedFiles.append(filePath);
            emit linkedFilesChanged();
        }
    }
}

void ChatRootView::onEditorCreated(Core::IEditor *editor, const Utils::FilePath &filePath)
{
    if (editor && editor->document()) {
        m_currentEditors.append(editor);
    }
}

QString ChatRootView::chatFileName() const
{
    return QFileInfo(m_recentFilePath).baseName();
}

void ChatRootView::setRecentFilePath(const QString &filePath)
{
    if (m_recentFilePath != filePath) {
        m_recentFilePath = filePath;
        emit chatFileNameChanged();
    }
}

bool ChatRootView::shouldIgnoreFileForAttach(const Utils::FilePath &filePath)
{
    auto project = ProjectExplorer::ProjectManager::projectForFile(filePath);
    if (project
        && m_clientInterface->contextManager()
               ->ignoreManager()
               ->shouldIgnore(filePath.toFSPathString(), project)) {
        LOG_MESSAGE(QString("Ignoring file for attachment due to .qodeassistignore: %1")
                        .arg(filePath.toFSPathString()));
        return true;
    }

    return false;
}

QString ChatRootView::textFontFamily() const
{
    return Settings::chatAssistantSettings().textFontFamily.stringValue();
}

QString ChatRootView::codeFontFamily() const
{
    return Settings::chatAssistantSettings().codeFontFamily.stringValue();
}

int ChatRootView::codeFontSize() const
{
    return Settings::chatAssistantSettings().codeFontSize();
}

int ChatRootView::textFontSize() const
{
    return Settings::chatAssistantSettings().textFontSize();
}

int ChatRootView::textFormat() const
{
    return Settings::chatAssistantSettings().textFormat();
}

bool ChatRootView::isRequestInProgress() const
{
    return m_isRequestInProgress;
}

void ChatRootView::setRequestProgressStatus(bool state)
{
    if (m_isRequestInProgress == state)
        return;
    m_isRequestInProgress = state;
    emit isRequestInProgressChanged();
}

} // namespace QodeAssist::Chat
