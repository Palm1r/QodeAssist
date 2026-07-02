// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatSerializer.hpp"
#include "Logger.hpp"

#include <memory>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

#include <LLMQore/ContentBlocks.hpp>

#include <ConversationHistory.hpp>
#include <Message.hpp>
#include <MessageSerializer.hpp>
#include <PluginBlocks.hpp>

#include "context/ChangesManager.h"

namespace QodeAssist::Chat {

namespace {

const QString kFileEditMarker = QStringLiteral("QODEASSIST_FILE_EDIT:");

enum class LegacyRole { System = 0, User = 1, Assistant = 2, Tool = 3, FileEdit = 4, Thinking = 5 };

void registerEditFromResult(const QString &result)
{
    const int pos = result.indexOf(kFileEditMarker);
    if (pos < 0)
        return;
    const QString jsonStr = result.mid(pos + kFileEditMarker.length());
    const QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isObject())
        return;
    const QJsonObject obj = doc.object();
    const QString editId = obj.value("edit_id").toString();
    const QString filePath = obj.value("file").toString();
    if (editId.isEmpty() || filePath.isEmpty())
        return;
    Context::ChangesManager::instance().addFileEdit(
        editId,
        filePath,
        obj.value("old_content").toString(),
        obj.value("new_content").toString(),
        false,
        true);
}

void appendMergingAssistants(std::vector<Message> &out, Message message)
{
    if (message.role() == Message::Role::Assistant && !out.empty()
        && out.back().role() == Message::Role::Assistant) {
        if (out.back().id().isEmpty() && !message.id().isEmpty())
            out.back().setId(message.id());
        for (auto &block : message.takeBlocks())
            out.back().appendBlock(std::move(block));
        return;
    }
    out.push_back(std::move(message));
}

} // namespace

const QString ChatSerializer::VERSION = "0.3";

SerializationResult ChatSerializer::saveToFile(
    const ConversationHistory *history, const QString &filePath, const QJsonObject &usage)
{
    if (!history)
        return {false, "No conversation history"};

    if (!ensureDirectoryExists(filePath)) {
        return {false, "Failed to create directory structure"};
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return {false, QString("Failed to open file for writing: %1").arg(filePath)};
    }

    QJsonDocument doc(serializeChat(history, usage));
    if (file.write(doc.toJson(QJsonDocument::Indented)) == -1) {
        return {false, QString("Failed to write to file: %1").arg(file.errorString())};
    }

    return {true, QString()};
}

SerializationResult ChatSerializer::loadFromFile(
    ConversationHistory *history, const QString &filePath, QJsonObject *usageOut)
{
    if (!history)
        return {false, "No conversation history"};

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {false, QString("Failed to open file for reading: %1").arg(filePath)};
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        return {false, QString("JSON parse error: %1").arg(error.errorString())};
    }

    const QJsonObject root = doc.object();
    const QString version = root["version"].toString();
    if (!validateVersion(version)) {
        return {false, QString("Unsupported version: %1").arg(version)};
    }

    if (version == VERSION)
        return loadCurrent(history, root, usageOut);
    return loadLegacy(history, root, usageOut);
}

QJsonObject ChatSerializer::serializeChat(
    const ConversationHistory *history, const QJsonObject &usage)
{
    QJsonArray messagesArray;
    for (const auto &message : history->messages())
        messagesArray.append(MessageSerializer::toJson(message));

    QJsonObject root;
    root["version"] = VERSION;
    root["messages"] = messagesArray;
    if (!usage.isEmpty())
        root["usage"] = usage;
    return root;
}

SerializationResult ChatSerializer::loadCurrent(
    ConversationHistory *history, const QJsonObject &root, QJsonObject *usageOut)
{
    history->clear();

    int skipped = 0;
    const QJsonArray messagesArray = root["messages"].toArray();
    for (const auto &value : messagesArray) {
        bool ok = false;
        Message message = MessageSerializer::fromJson(value.toObject(), &ok);
        if (ok)
            history->append(std::move(message));
        else
            ++skipped;
    }

    if (usageOut)
        *usageOut = root["usage"].toObject();

    registerHistoricalFileEdits(history);
    if (skipped > 0) {
        return {true, QString("%1 message(s) could not be parsed and were skipped").arg(skipped)};
    }
    return {true, QString()};
}

SerializationResult ChatSerializer::loadLegacy(
    ConversationHistory *history, const QJsonObject &root, QJsonObject *usageOut)
{
    history->clear();

    QJsonObject usage;
    const auto collectUsage = [&usage](const QJsonObject &mj) {
        const QString id = mj["id"].toString();
        const QJsonObject legacyUsage = mj["usage"].toObject();
        if (id.isEmpty() || legacyUsage.isEmpty())
            return;
        QJsonObject entry;
        entry["prompt"] = legacyUsage["promptTokens"].toInt();
        entry["completion"] = legacyUsage["completionTokens"].toInt();
        entry["cached"] = legacyUsage["cachedPromptTokens"].toInt();
        entry["reasoning"] = legacyUsage["reasoningTokens"].toInt();
        usage.insert(id, entry);
    };

    std::vector<Message> merged;
    const QJsonArray arr = root["messages"].toArray();
    int i = 0;
    while (i < arr.size()) {
        const QJsonObject mj = arr[i].toObject();
        const auto role = static_cast<LegacyRole>(mj["role"].toInt());

        if (role == LegacyRole::Tool) {
            Message assistant(Message::Role::Assistant);
            Message toolResults(Message::Role::User);
            while (i < arr.size()
                   && static_cast<LegacyRole>(arr[i].toObject()["role"].toInt())
                          == LegacyRole::Tool) {
                const QJsonObject tj = arr[i].toObject();
                const QString toolName = tj["toolName"].toString();
                const QString id = tj["id"].toString();
                if (!toolName.isEmpty()) {
                    assistant.appendBlock(
                        std::make_unique<LLMQore::ToolUseContent>(
                            id, toolName, tj["toolArguments"].toObject()));
                    toolResults.appendBlock(
                        std::make_unique<LLMQore::ToolResultContent>(id, tj["toolResult"].toString()));
                }
                ++i;
            }
            if (!assistant.blocks().empty()) {
                appendMergingAssistants(merged, std::move(assistant));
                merged.push_back(std::move(toolResults));
            }
            continue;
        }

        ++i;

        if (role == LegacyRole::FileEdit)
            continue;

        if (role == LegacyRole::Thinking) {
            const QString content = mj["content"].toString();
            const QString signature = mj["signature"].toString();
            Message assistant(Message::Role::Assistant);
            if (mj["isRedacted"].toBool(false)) {
                assistant.appendBlock(std::make_unique<LLMQore::RedactedThinkingContent>(signature));
            } else {
                const int sigPos = content.indexOf(QStringLiteral("\n[Signature:"));
                const QString thinking = sigPos >= 0 ? content.left(sigPos) : content;
                assistant.appendBlock(
                    std::make_unique<LLMQore::ThinkingContent>(thinking, signature));
            }
            appendMergingAssistants(merged, std::move(assistant));
            continue;
        }

        if (role == LegacyRole::User) {
            Message user(Message::Role::User, mj["id"].toString());
            user.appendBlock(std::make_unique<LLMQore::TextContent>(mj["content"].toString()));
            for (const auto &a : mj["attachments"].toArray()) {
                const QJsonObject ao = a.toObject();
                user.appendBlock(
                    std::make_unique<StoredAttachmentContent>(
                        ao["fileName"].toString(), ao["storedPath"].toString()));
            }
            for (const auto &im : mj["images"].toArray()) {
                const QJsonObject io = im.toObject();
                user.appendBlock(
                    std::make_unique<StoredImageContent>(
                        io["fileName"].toString(),
                        io["storedPath"].toString(),
                        io["mediaType"].toString()));
            }
            merged.push_back(std::move(user));
        } else {
            const QString content = mj["content"].toString();
            if (content.trimmed().isEmpty())
                continue;
            const Message::Role mapped = role == LegacyRole::System ? Message::Role::System
                                                                    : Message::Role::Assistant;
            Message message(mapped, mj["id"].toString());
            message.appendBlock(std::make_unique<LLMQore::TextContent>(content));
            collectUsage(mj);
            if (mapped == Message::Role::Assistant)
                appendMergingAssistants(merged, std::move(message));
            else
                merged.push_back(std::move(message));
        }
    }

    for (auto &message : merged)
        history->append(std::move(message));

    if (usageOut)
        *usageOut = usage;

    registerHistoricalFileEdits(history);
    return {true, QString()};
}

void ChatSerializer::registerHistoricalFileEdits(const ConversationHistory *history)
{
    for (const auto &message : history->messages()) {
        for (const auto &block : message.blocks()) {
            if (auto *tr = dynamic_cast<LLMQore::ToolResultContent *>(block.get()))
                registerEditFromResult(tr->result());
        }
    }
}

bool ChatSerializer::ensureDirectoryExists(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    return dir.exists() || dir.mkpath(".");
}

bool ChatSerializer::validateVersion(const QString &version)
{
    return version == VERSION || version == "0.2" || version == "0.1";
}

QString ChatSerializer::getChatContentFolder(const QString &chatFilePath)
{
    QFileInfo fileInfo(chatFilePath);
    QString baseName = fileInfo.completeBaseName();
    QString dirPath = fileInfo.absolutePath();
    return QDir(dirPath).filePath(baseName + "_content");
}

bool ChatSerializer::saveContentToStorage(
    const QString &chatFilePath,
    const QString &fileName,
    const QString &base64Data,
    QString &storedPath)
{
    QString contentFolder = getChatContentFolder(chatFilePath);
    QDir dir;
    if (!dir.exists(contentFolder)) {
        if (!dir.mkpath(contentFolder)) {
            LOG_MESSAGE(QString("Failed to create content folder: %1").arg(contentFolder));
            return false;
        }
    }

    QFileInfo originalFileInfo(fileName);
    QString extension = originalFileInfo.suffix();
    QString baseName = originalFileInfo.completeBaseName();
    QString uniqueName = QString("%1_%2.%3")
                             .arg(baseName)
                             .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8))
                             .arg(extension);

    QString fullPath = QDir(contentFolder).filePath(uniqueName);

    QByteArray contentData = QByteArray::fromBase64(base64Data.toUtf8());
    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_MESSAGE(QString("Failed to open file for writing: %1").arg(fullPath));
        return false;
    }

    if (file.write(contentData) == -1) {
        LOG_MESSAGE(QString("Failed to write content data: %1").arg(file.errorString()));
        return false;
    }

    file.close();

    storedPath = uniqueName;
    LOG_MESSAGE(QString("Saved content: %1 to %2").arg(fileName, fullPath));

    return true;
}

QString ChatSerializer::loadContentFromStorage(
    const QString &chatFilePath, const QString &storedPath, StoredContentCache *cache)
{
    const QString contentFolder = getChatContentFolder(chatFilePath);
    const QString fullPath = QDir(contentFolder).filePath(storedPath);

    const QFileInfo info(fullPath);
    if (cache) {
        const auto it = cache->constFind(fullPath);
        if (it != cache->constEnd() && it->modified == info.lastModified()
            && it->size == info.size()) {
            return it->base64;
        }
    }

    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_MESSAGE(QString("Failed to open content file: %1").arg(fullPath));
        return QString();
    }

    const QString base64 = QString::fromUtf8(file.readAll().toBase64());

    if (cache)
        cache->insert(fullPath, {info.lastModified(), info.size(), base64});

    return base64;
}

} // namespace QodeAssist::Chat
