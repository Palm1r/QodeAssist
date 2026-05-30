// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>

#include <LLMQore/ContentBlocks.hpp>

#include <Message.hpp>
#include <MessageSerializer.hpp>
#include <PluginBlocks.hpp>

using namespace QodeAssist;

namespace {

// Round-trips a message through JSON and back, returning the re-serialized
// form so it can be compared against the original serialization. Any field
// dropped or mangled by fromJson/toJson surfaces as a JSON mismatch.
QJsonObject reserialize(const Message &message)
{
    bool ok = false;
    const QJsonObject json = MessageSerializer::toJson(message);
    Message restored = MessageSerializer::fromJson(json, &ok);
    EXPECT_TRUE(ok);
    return MessageSerializer::toJson(restored);
}

} // namespace

TEST(MessageSerializerTest, RoleAndIdRoundtrip)
{
    Message m(Message::Role::Assistant, QStringLiteral("msg-7"));
    m.appendBlock(std::make_unique<LLMQore::TextContent>(QStringLiteral("hi")));

    const QJsonObject json = MessageSerializer::toJson(m);
    EXPECT_EQ(json.value("role").toString(), QStringLiteral("assistant"));
    EXPECT_EQ(json.value("id").toString(), QStringLiteral("msg-7"));

    EXPECT_EQ(reserialize(m), json);
}

TEST(MessageSerializerTest, EmptyIdIsOmitted)
{
    Message m(Message::Role::User);
    m.appendBlock(std::make_unique<LLMQore::TextContent>(QStringLiteral("x")));

    const QJsonObject json = MessageSerializer::toJson(m);
    EXPECT_FALSE(json.contains(QStringLiteral("id")));
    EXPECT_EQ(json.value("role").toString(), QStringLiteral("user"));
}

TEST(MessageSerializerTest, SystemRoleRoundtrip)
{
    Message m(Message::Role::System);
    m.appendBlock(std::make_unique<LLMQore::TextContent>(QStringLiteral("rules")));
    EXPECT_EQ(MessageSerializer::toJson(m).value("role").toString(), QStringLiteral("system"));
    EXPECT_EQ(reserialize(m), MessageSerializer::toJson(m));
}

TEST(MessageSerializerTest, ThinkingBlockPreservesSignature)
{
    Message m(Message::Role::Assistant);
    m.appendBlock(
        std::make_unique<LLMQore::ThinkingContent>(QStringLiteral("draft"), QStringLiteral("sig")));

    const QJsonObject block
        = MessageSerializer::toJson(m).value("blocks").toArray().first().toObject();
    EXPECT_EQ(block.value("type").toString(), QStringLiteral("thinking"));
    EXPECT_EQ(block.value("thinking").toString(), QStringLiteral("draft"));
    EXPECT_EQ(block.value("signature").toString(), QStringLiteral("sig"));

    EXPECT_EQ(reserialize(m), MessageSerializer::toJson(m));
}

TEST(MessageSerializerTest, RedactedThinkingRoundtrip)
{
    Message m(Message::Role::Assistant);
    m.appendBlock(std::make_unique<LLMQore::RedactedThinkingContent>(QStringLiteral("blob")));

    const QJsonObject block
        = MessageSerializer::toJson(m).value("blocks").toArray().first().toObject();
    EXPECT_EQ(block.value("type").toString(), QStringLiteral("redacted_thinking"));
    EXPECT_EQ(block.value("signature").toString(), QStringLiteral("blob"));

    EXPECT_EQ(reserialize(m), MessageSerializer::toJson(m));
}

TEST(MessageSerializerTest, ImageBase64Roundtrip)
{
    Message m(Message::Role::User);
    m.appendBlock(std::make_unique<LLMQore::ImageContent>(
        QStringLiteral("ZGF0YQ=="),
        QStringLiteral("image/png"),
        LLMQore::ImageContent::ImageSourceType::Base64));

    const QJsonObject block
        = MessageSerializer::toJson(m).value("blocks").toArray().first().toObject();
    EXPECT_EQ(block.value("type").toString(), QStringLiteral("image"));
    EXPECT_EQ(block.value("sourceType").toString(), QStringLiteral("base64"));
    EXPECT_EQ(block.value("mediaType").toString(), QStringLiteral("image/png"));

    EXPECT_EQ(reserialize(m), MessageSerializer::toJson(m));
}

TEST(MessageSerializerTest, ImageUrlSourceTypeRoundtrip)
{
    Message m(Message::Role::User);
    m.appendBlock(std::make_unique<LLMQore::ImageContent>(
        QStringLiteral("https://example.com/a.png"),
        QStringLiteral("image/png"),
        LLMQore::ImageContent::ImageSourceType::Url));

    const QJsonObject block
        = MessageSerializer::toJson(m).value("blocks").toArray().first().toObject();
    EXPECT_EQ(block.value("sourceType").toString(), QStringLiteral("url"));

    EXPECT_EQ(reserialize(m), MessageSerializer::toJson(m));
}

TEST(MessageSerializerTest, ToolUseRoundtrip)
{
    Message m(Message::Role::Assistant);
    m.appendBlock(std::make_unique<LLMQore::ToolUseContent>(
        QStringLiteral("tu1"),
        QStringLiteral("read_file"),
        QJsonObject{{"path", "a.cpp"}}));

    const QJsonObject block
        = MessageSerializer::toJson(m).value("blocks").toArray().first().toObject();
    EXPECT_EQ(block.value("type").toString(), QStringLiteral("tool_use"));
    EXPECT_EQ(block.value("id").toString(), QStringLiteral("tu1"));
    EXPECT_EQ(block.value("name").toString(), QStringLiteral("read_file"));
    EXPECT_EQ(block.value("input").toObject().value("path").toString(), QStringLiteral("a.cpp"));

    EXPECT_EQ(reserialize(m), MessageSerializer::toJson(m));
}

TEST(MessageSerializerTest, ToolResultRoundtrip)
{
    Message m(Message::Role::User);
    m.appendBlock(
        std::make_unique<LLMQore::ToolResultContent>(QStringLiteral("tu1"), QStringLiteral("body")));

    const QJsonObject block
        = MessageSerializer::toJson(m).value("blocks").toArray().first().toObject();
    EXPECT_EQ(block.value("type").toString(), QStringLiteral("tool_result"));
    EXPECT_EQ(block.value("toolUseId").toString(), QStringLiteral("tu1"));
    EXPECT_EQ(block.value("result").toString(), QStringLiteral("body"));

    EXPECT_EQ(reserialize(m), MessageSerializer::toJson(m));
}

TEST(MessageSerializerTest, StoredImageRoundtrip)
{
    Message m(Message::Role::User);
    m.appendBlock(std::make_unique<StoredImageContent>(
        QStringLiteral("shot.png"), QStringLiteral("stored/shot"), QStringLiteral("image/png")));

    const QJsonObject block
        = MessageSerializer::toJson(m).value("blocks").toArray().first().toObject();
    EXPECT_EQ(block.value("type").toString(), QStringLiteral("stored_image"));
    EXPECT_EQ(block.value("fileName").toString(), QStringLiteral("shot.png"));
    EXPECT_EQ(block.value("storedPath").toString(), QStringLiteral("stored/shot"));
    EXPECT_EQ(block.value("mediaType").toString(), QStringLiteral("image/png"));

    EXPECT_EQ(reserialize(m), MessageSerializer::toJson(m));
}

TEST(MessageSerializerTest, StoredAttachmentRoundtrip)
{
    Message m(Message::Role::User);
    m.appendBlock(std::make_unique<StoredAttachmentContent>(
        QStringLiteral("notes.txt"), QStringLiteral("stored/notes")));

    const QJsonObject block
        = MessageSerializer::toJson(m).value("blocks").toArray().first().toObject();
    EXPECT_EQ(block.value("type").toString(), QStringLiteral("stored_attachment"));
    EXPECT_EQ(block.value("fileName").toString(), QStringLiteral("notes.txt"));
    EXPECT_EQ(block.value("storedPath").toString(), QStringLiteral("stored/notes"));

    EXPECT_EQ(reserialize(m), MessageSerializer::toJson(m));
}

TEST(MessageSerializerTest, SkillInvocationRoundtrip)
{
    Message m(Message::Role::User);
    m.appendBlock(std::make_unique<SkillInvocationContent>(
        QStringLiteral("review"), QStringLiteral("Review the code.")));

    const QJsonObject block
        = MessageSerializer::toJson(m).value("blocks").toArray().first().toObject();
    EXPECT_EQ(block.value("type").toString(), QStringLiteral("skill_invocation"));
    EXPECT_EQ(block.value("skillName").toString(), QStringLiteral("review"));
    EXPECT_EQ(block.value("body").toString(), QStringLiteral("Review the code."));

    EXPECT_EQ(reserialize(m), MessageSerializer::toJson(m));
}

TEST(MessageSerializerTest, FileEditRoundtripWithStatusAndMessage)
{
    Message m(Message::Role::Assistant);
    m.appendBlock(std::make_unique<FileEditContent>(
        QStringLiteral("e1"),
        QStringLiteral("a.cpp"),
        QStringLiteral("old"),
        QStringLiteral("new"),
        FileEditContent::Status::Applied,
        QStringLiteral("done")));

    const QJsonObject block
        = MessageSerializer::toJson(m).value("blocks").toArray().first().toObject();
    EXPECT_EQ(block.value("type").toString(), QStringLiteral("file_edit"));
    EXPECT_EQ(block.value("editId").toString(), QStringLiteral("e1"));
    EXPECT_EQ(block.value("filePath").toString(), QStringLiteral("a.cpp"));
    EXPECT_EQ(block.value("oldContent").toString(), QStringLiteral("old"));
    EXPECT_EQ(block.value("newContent").toString(), QStringLiteral("new"));
    EXPECT_EQ(block.value("status").toString(), QStringLiteral("applied"));
    EXPECT_EQ(block.value("statusMessage").toString(), QStringLiteral("done"));

    EXPECT_EQ(reserialize(m), MessageSerializer::toJson(m));
}

TEST(MessageSerializerTest, FileEditOmitsEmptyStatusMessageAndDefaultsToPending)
{
    Message m(Message::Role::Assistant);
    m.appendBlock(std::make_unique<FileEditContent>(
        QStringLiteral("e1"),
        QStringLiteral("a.cpp"),
        QStringLiteral("old"),
        QStringLiteral("new")));

    const QJsonObject block
        = MessageSerializer::toJson(m).value("blocks").toArray().first().toObject();
    EXPECT_EQ(block.value("status").toString(), QStringLiteral("pending"));
    EXPECT_FALSE(block.contains(QStringLiteral("statusMessage")));
}

TEST(MessageSerializerTest, MultipleBlocksPreserveOrder)
{
    Message m(Message::Role::Assistant);
    m.appendBlock(std::make_unique<LLMQore::TextContent>(QStringLiteral("calling")));
    m.appendBlock(std::make_unique<LLMQore::ToolUseContent>(
        QStringLiteral("tu1"), QStringLiteral("read_file"), QJsonObject()));

    const QJsonArray blocks = MessageSerializer::toJson(m).value("blocks").toArray();
    ASSERT_EQ(blocks.size(), 2);
    EXPECT_EQ(blocks[0].toObject().value("type").toString(), QStringLiteral("text"));
    EXPECT_EQ(blocks[1].toObject().value("type").toString(), QStringLiteral("tool_use"));

    EXPECT_EQ(reserialize(m), MessageSerializer::toJson(m));
}

TEST(MessageSerializerTest, UnknownRoleFailsDeserialization)
{
    QJsonObject json;
    json["role"] = QStringLiteral("operator");
    json["blocks"] = QJsonArray{};

    bool ok = true;
    const Message m = MessageSerializer::fromJson(json, &ok);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(m.blocks().empty());
}

TEST(MessageSerializerTest, EmptyBlocksDeserializeOk)
{
    QJsonObject json;
    json["role"] = QStringLiteral("user");
    json["blocks"] = QJsonArray{};

    bool ok = false;
    const Message m = MessageSerializer::fromJson(json, &ok);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(m.blocks().empty());
}

TEST(MessageSerializerTest, AllUnknownBlocksFailDeserialization)
{
    QJsonObject json;
    json["role"] = QStringLiteral("assistant");
    json["blocks"] = QJsonArray{QJsonObject{{"type", "future_block"}}};

    bool ok = true;
    const Message m = MessageSerializer::fromJson(json, &ok);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(m.blocks().empty());
}

TEST(MessageSerializerTest, UnknownBlocksSkippedButKnownKept)
{
    QJsonObject json;
    json["role"] = QStringLiteral("assistant");
    json["blocks"] = QJsonArray{
        QJsonObject{{"type", "future_block"}},
        QJsonObject{{"type", "text"}, {"text", "kept"}}};

    bool ok = false;
    const Message m = MessageSerializer::fromJson(json, &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(m.blocks().size(), 1u);
    EXPECT_EQ(m.text(), QStringLiteral("kept"));
}
