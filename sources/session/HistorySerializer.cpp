// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/HistorySerializer.hpp"

#include <QJsonArray>

#include "session/HistoryProjection.hpp"

namespace QodeAssist::Session {

namespace {

QString roleToString(MessageRole role)
{
    switch (role) {
    case MessageRole::System:
        return QStringLiteral("system");
    case MessageRole::User:
        return QStringLiteral("user");
    case MessageRole::Assistant:
        return QStringLiteral("assistant");
    }
    return QStringLiteral("assistant");
}

MessageRole roleFromString(const QString &role)
{
    if (role == QLatin1String("system"))
        return MessageRole::System;
    if (role == QLatin1String("user"))
        return MessageRole::User;
    return MessageRole::Assistant;
}

RowKind legacyRowKind(int role)
{
    switch (role) {
    case 0:
        return RowKind::System;
    case 1:
        return RowKind::User;
    case 2:
        return RowKind::Assistant;
    case 3:
        return RowKind::Tool;
    case 4:
        return RowKind::FileEdit;
    case 5:
        return RowKind::Thinking;
    default:
        return RowKind::Assistant;
    }
}

QJsonObject usageToJson(const Usage &usage)
{
    QJsonObject json;
    json["promptTokens"] = usage.promptTokens;
    json["completionTokens"] = usage.completionTokens;
    if (usage.cachedPromptTokens > 0)
        json["cachedPromptTokens"] = usage.cachedPromptTokens;
    if (usage.reasoningTokens > 0)
        json["reasoningTokens"] = usage.reasoningTokens;
    return json;
}

Usage usageFromJson(const QJsonObject &json)
{
    Usage usage;
    usage.promptTokens = json["promptTokens"].toInt();
    usage.completionTokens = json["completionTokens"].toInt();
    usage.cachedPromptTokens = json["cachedPromptTokens"].toInt();
    usage.reasoningTokens = json["reasoningTokens"].toInt();
    return usage;
}

QJsonObject blockToJson(const ContentBlock &block)
{
    static_assert(
        std::variant_size_v<ContentBlock> == 8,
        "ContentBlock gained an alternative; extend blockToJson and blockFromJson below or it is "
        "written without a type and dropped on the next load");

    QJsonObject json;

    if (const auto *text = std::get_if<TextBlock>(&block)) {
        json["type"] = "text";
        json["text"] = text->text;
    } else if (const auto *thinking = std::get_if<ThinkingBlock>(&block)) {
        json["type"] = "thinking";
        json["text"] = thinking->text;
        if (!thinking->signature.isEmpty())
            json["signature"] = thinking->signature;
        if (thinking->redacted)
            json["redacted"] = true;
    } else if (const auto *tool = std::get_if<ToolCallBlock>(&block)) {
        json["type"] = "tool_call";
        json["id"] = tool->id;
        json["name"] = tool->name;
        if (!tool->arguments.isEmpty())
            json["arguments"] = tool->arguments;
        if (!tool->result.isEmpty())
            json["result"] = tool->result;
        if (!tool->kind.isEmpty())
            json["kind"] = tool->kind;
        if (!tool->status.isEmpty())
            json["status"] = tool->status;
        if (!tool->details.isEmpty())
            json["details"] = tool->details;
        if (tool->fromAgent)
            json["fromAgent"] = true;
    } else if (const auto *attachment = std::get_if<AttachmentBlock>(&block)) {
        json["type"] = "attachment";
        json["fileName"] = attachment->fileName;
        json["storedPath"] = attachment->storedPath;
    } else if (const auto *image = std::get_if<ImageBlock>(&block)) {
        json["type"] = "image";
        json["fileName"] = image->fileName;
        json["storedPath"] = image->storedPath;
        json["mediaType"] = image->mediaType;
    } else if (const auto *edit = std::get_if<FileEditBlock>(&block)) {
        json["type"] = "file_edit";
        json["id"] = edit->id;
        json["payload"] = edit->payload;
    } else if (const auto *permission = std::get_if<PermissionBlock>(&block)) {
        json = permissionBlockToJson(*permission);
        json["type"] = "permission";
    } else if (const auto *plan = std::get_if<PlanBlock>(&block)) {
        json = planBlockToJson(*plan);
        json["type"] = "plan";
    }

    return json;
}

std::optional<ContentBlock> blockFromJson(const QJsonObject &json)
{
    const QString type = json["type"].toString();

    if (type == QLatin1String("text"))
        return ContentBlock{TextBlock{json["text"].toString()}};

    if (type == QLatin1String("thinking")) {
        return ContentBlock{ThinkingBlock{
            json["text"].toString(), json["signature"].toString(), json["redacted"].toBool(false)}};
    }

    if (type == QLatin1String("tool_call")) {
        return ContentBlock{ToolCallBlock{
            json["id"].toString(),
            json["name"].toString(),
            json["arguments"].toObject(),
            json["result"].toString(),
            json["kind"].toString(),
            json["status"].toString(),
            json["details"].toObject(),
            json["fromAgent"].toBool(false)}};
    }

    if (type == QLatin1String("attachment"))
        return ContentBlock{
            AttachmentBlock{json["fileName"].toString(), json["storedPath"].toString()}};

    if (type == QLatin1String("image")) {
        return ContentBlock{ImageBlock{
            json["fileName"].toString(),
            json["storedPath"].toString(),
            json["mediaType"].toString()}};
    }

    if (type == QLatin1String("file_edit"))
        return ContentBlock{FileEditBlock{json["id"].toString(), json["payload"].toString()}};

    if (type == QLatin1String("permission"))
        return ContentBlock{restoredPermissionBlock(permissionBlockFromJson(json))};

    if (type == QLatin1String("plan"))
        return ContentBlock{planBlockFromJson(json)};

    return std::nullopt;
}

QJsonObject messageToJson(const Message &message)
{
    QJsonArray blocks;
    for (const ContentBlock &block : message.blocks)
        blocks.append(blockToJson(block));

    QJsonObject json;
    json["role"] = roleToString(message.role);
    if (!message.id.isEmpty())
        json["id"] = message.id;
    json["blocks"] = blocks;
    if (!message.usage.isEmpty())
        json["usage"] = usageToJson(message.usage);

    return json;
}

Message messageFromJson(const QJsonObject &json, int &droppedBlocks)
{
    Message message;
    message.role = roleFromString(json["role"].toString());
    message.id = json["id"].toString();

    const QJsonArray blocks = json["blocks"].toArray();
    for (const QJsonValue &value : blocks) {
        if (auto block = blockFromJson(value.toObject()))
            message.blocks.append(*block);
        else
            ++droppedBlocks;
    }

    if (json.contains("usage"))
        message.usage = usageFromJson(json["usage"].toObject());

    return message;
}

MessageRow legacyRowFromJson(const QJsonObject &json)
{
    MessageRow row;
    row.kind = legacyRowKind(json["role"].toInt());
    row.id = json["id"].toString();
    row.content = json["content"].toString();
    row.redacted = json["isRedacted"].toBool(false);
    row.signature = json["signature"].toString();
    row.toolName = json["toolName"].toString();
    row.toolArguments = json["toolArguments"].toObject();
    row.toolResult = json["toolResult"].toString();

    const QJsonArray attachments = json["attachments"].toArray();
    for (const QJsonValue &value : attachments) {
        const QJsonObject attachment = value.toObject();
        row.attachments.append(
            AttachmentBlock{attachment["fileName"].toString(), attachment["storedPath"].toString()});
    }

    const QJsonArray images = json["images"].toArray();
    for (const QJsonValue &value : images) {
        const QJsonObject image = value.toObject();
        row.images.append(
            ImageBlock{
                image["fileName"].toString(),
                image["storedPath"].toString(),
                image["mediaType"].toString()});
    }

    if (json.contains("usage"))
        row.usage = usageFromJson(json["usage"].toObject());

    return row;
}

ConversationHistory historyFromLegacyJson(const QJsonObject &root)
{
    QList<MessageRow> rows;
    const QJsonArray messages = root["messages"].toArray();
    for (const QJsonValue &value : messages)
        rows.append(legacyRowFromJson(value.toObject()));

    return buildFromRows(rows);
}

} // namespace

QString HistorySerializer::currentVersion()
{
    return QStringLiteral("0.4");
}

bool HistorySerializer::isSupportedVersion(const QString &version)
{
    return version == currentVersion() || version == QLatin1String("0.2")
           || version == QLatin1String("0.1");
}

QJsonObject HistorySerializer::toJson(const ConversationHistory &history)
{
    QJsonArray messages;
    for (const Message &message : history.messages())
        messages.append(messageToJson(message));

    QJsonObject root;
    root["version"] = currentVersion();
    root["messages"] = messages;

    return root;
}

std::optional<ConversationHistory> HistorySerializer::fromJson(
    const QJsonObject &root, int *droppedBlocks)
{
    if (droppedBlocks)
        *droppedBlocks = 0;

    const QString version = root["version"].toString();

    if (!isSupportedVersion(version) || !root["messages"].isArray())
        return std::nullopt;

    if (version != currentVersion())
        return historyFromLegacyJson(root);

    int dropped = 0;
    ConversationHistory history;
    const QJsonArray messages = root["messages"].toArray();
    for (const QJsonValue &value : messages)
        history.append(messageFromJson(value.toObject(), dropped));

    if (droppedBlocks)
        *droppedBlocks = dropped;

    return history;
}

} // namespace QodeAssist::Session
