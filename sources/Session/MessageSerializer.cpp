// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "MessageSerializer.hpp"

#include "PluginBlocks.hpp"

#include <LLMQore/ContentBlocks.hpp>

#include <QDebug>
#include <QJsonArray>

#include <memory>

namespace QodeAssist {

namespace {

constexpr auto kKindText = "text";
constexpr auto kKindThinking = "thinking";
constexpr auto kKindRedactedThinking = "redacted_thinking";
constexpr auto kKindImage = "image";
constexpr auto kKindToolUse = "tool_use";
constexpr auto kKindToolResult = "tool_result";
constexpr auto kKindStoredImage = "stored_image";
constexpr auto kKindStoredAttachment = "stored_attachment";
constexpr auto kKindFileEdit = "file_edit";

QString roleToString(Message::Role role)
{
    switch (role) {
    case Message::Role::System: return QStringLiteral("system");
    case Message::Role::User: return QStringLiteral("user");
    case Message::Role::Assistant: return QStringLiteral("assistant");
    }
    return QStringLiteral("user");
}

bool roleFromString(const QString &str, Message::Role *out)
{
    if (str == QLatin1String("system")) {
        *out = Message::Role::System;
        return true;
    }
    if (str == QLatin1String("user")) {
        *out = Message::Role::User;
        return true;
    }
    if (str == QLatin1String("assistant")) {
        *out = Message::Role::Assistant;
        return true;
    }
    return false;
}

QJsonObject blockToJson(const LLMQore::ContentBlock &block)
{
    QJsonObject obj;
    if (auto *t = dynamic_cast<const LLMQore::TextContent *>(&block)) {
        obj["type"] = kKindText;
        obj["text"] = t->text();
    } else if (auto *th = dynamic_cast<const LLMQore::ThinkingContent *>(&block)) {
        obj["type"] = kKindThinking;
        obj["thinking"] = th->thinking();
        obj["signature"] = th->signature();
    } else if (auto *rth = dynamic_cast<const LLMQore::RedactedThinkingContent *>(&block)) {
        obj["type"] = kKindRedactedThinking;
        obj["signature"] = rth->signature();
    } else if (auto *img = dynamic_cast<const LLMQore::ImageContent *>(&block)) {
        obj["type"] = kKindImage;
        obj["data"] = img->data();
        obj["mediaType"] = img->mediaType();
        obj["sourceType"] = (img->sourceType() == LLMQore::ImageContent::ImageSourceType::Url)
                                ? QStringLiteral("url")
                                : QStringLiteral("base64");
    } else if (auto *tu = dynamic_cast<const LLMQore::ToolUseContent *>(&block)) {
        obj["type"] = kKindToolUse;
        obj["id"] = tu->id();
        obj["name"] = tu->name();
        obj["input"] = tu->input();
    } else if (auto *tr = dynamic_cast<const LLMQore::ToolResultContent *>(&block)) {
        obj["type"] = kKindToolResult;
        obj["toolUseId"] = tr->toolUseId();
        obj["result"] = tr->result();
    } else if (auto *si = dynamic_cast<const StoredImageContent *>(&block)) {
        obj["type"] = kKindStoredImage;
        obj["fileName"] = si->fileName();
        obj["storedPath"] = si->storedPath();
        obj["mediaType"] = si->mediaType();
    } else if (auto *sa = dynamic_cast<const StoredAttachmentContent *>(&block)) {
        obj["type"] = kKindStoredAttachment;
        obj["fileName"] = sa->fileName();
        obj["storedPath"] = sa->storedPath();
    } else if (auto *fe = dynamic_cast<const FileEditContent *>(&block)) {
        obj["type"] = kKindFileEdit;
        obj["editId"] = fe->editId();
        obj["filePath"] = fe->filePath();
        obj["oldContent"] = fe->oldContent();
        obj["newContent"] = fe->newContent();
        obj["status"] = FileEditContent::statusToString(fe->status());
        if (!fe->statusMessage().isEmpty())
            obj["statusMessage"] = fe->statusMessage();
    }
    return obj;
}

std::unique_ptr<LLMQore::ContentBlock> blockFromJson(const QJsonObject &obj)
{
    const QString type = obj.value("type").toString();
    if (type == kKindText) {
        return std::make_unique<LLMQore::TextContent>(obj.value("text").toString());
    }
    if (type == kKindThinking) {
        return std::make_unique<LLMQore::ThinkingContent>(
            obj.value("thinking").toString(), obj.value("signature").toString());
    }
    if (type == kKindRedactedThinking) {
        return std::make_unique<LLMQore::RedactedThinkingContent>(
            obj.value("signature").toString());
    }
    if (type == kKindImage) {
        const auto sourceType
            = (obj.value("sourceType").toString() == QLatin1String("url"))
                  ? LLMQore::ImageContent::ImageSourceType::Url
                  : LLMQore::ImageContent::ImageSourceType::Base64;
        return std::make_unique<LLMQore::ImageContent>(
            obj.value("data").toString(), obj.value("mediaType").toString(), sourceType);
    }
    if (type == kKindToolUse) {
        return std::make_unique<LLMQore::ToolUseContent>(
            obj.value("id").toString(),
            obj.value("name").toString(),
            obj.value("input").toObject());
    }
    if (type == kKindToolResult) {
        return std::make_unique<LLMQore::ToolResultContent>(
            obj.value("toolUseId").toString(), obj.value("result").toString());
    }
    if (type == kKindStoredImage) {
        return std::make_unique<StoredImageContent>(
            obj.value("fileName").toString(),
            obj.value("storedPath").toString(),
            obj.value("mediaType").toString());
    }
    if (type == kKindStoredAttachment) {
        return std::make_unique<StoredAttachmentContent>(
            obj.value("fileName").toString(), obj.value("storedPath").toString());
    }
    if (type == kKindFileEdit) {
        return std::make_unique<FileEditContent>(
            obj.value("editId").toString(),
            obj.value("filePath").toString(),
            obj.value("oldContent").toString(),
            obj.value("newContent").toString(),
            FileEditContent::statusFromString(obj.value("status").toString()),
            obj.value("statusMessage").toString());
    }
    return nullptr; // unknown type — skipped
}

} // namespace

QJsonObject MessageSerializer::toJson(const Message &message)
{
    QJsonObject obj;
    obj["role"] = roleToString(message.role());
    if (!message.id().isEmpty())
        obj["id"] = message.id();

    QJsonArray blocks;
    for (const auto &b : message.blocks()) {
        if (b)
            blocks.append(blockToJson(*b));
    }
    obj["blocks"] = blocks;
    return obj;
}

Message MessageSerializer::fromJson(const QJsonObject &json, bool *ok)
{
    Message::Role role;
    if (!roleFromString(json.value("role").toString(), &role)) {
        if (ok)
            *ok = false;
        return Message();
    }

    Message m(role, json.value("id").toString());
    const QJsonArray blocks = json.value("blocks").toArray();
    int unknownBlocks = 0;
    for (const QJsonValue &v : blocks) {
        const QJsonObject blockObj = v.toObject();
        auto block = blockFromJson(blockObj);
        if (block) {
            m.appendBlock(std::move(block));
        } else {
            ++unknownBlocks;
            qWarning("[QodeAssist] MessageSerializer: unknown block type '%s' "
                     "in stored chat — skipped",
                     qUtf8Printable(blockObj.value("type").toString()));
        }
    }

    if (ok) {
        *ok = m.blocks().size() > 0 || blocks.isEmpty();
        if (unknownBlocks > 0 && !blocks.isEmpty() && m.blocks().empty())
            *ok = false;
    }
    return m;
}

} // namespace QodeAssist
