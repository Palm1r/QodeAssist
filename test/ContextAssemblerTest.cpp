// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include <gtest/gtest.h>

#include <QJsonObject>
#include <QString>

#include <LLMQore/ContentBlocks.hpp>

#include <ContextAssembler.hpp>
#include <Message.hpp>
#include <PluginBlocks.hpp>

using namespace QodeAssist;
using Templates::ContentBlockEntry;

namespace {

Message textMessage(Message::Role role, const QString &text)
{
    Message m(role);
    m.appendBlock(std::make_unique<LLMQore::TextContent>(text));
    return m;
}

ContextAssembler::ContentLoader base64Loader(const QString &content)
{
    return [content](const QString &) {
        return QString::fromUtf8(content.toUtf8().toBase64());
    };
}

ContextAssembler::ContentLoader emptyLoader()
{
    return [](const QString &) { return QString(); };
}

} // namespace

TEST(ContextAssemblerTest, SystemPromptAndUserTextProduceWireContext)
{
    std::vector<Message> history;
    history.push_back(textMessage(Message::Role::User, "hello"));

    ContextAssembler::Manifest manifest;
    const auto ctx = ContextAssembler::assemble(history, "be helpful", nullptr, {}, &manifest);

    ASSERT_TRUE(ctx.systemPrompt.has_value());
    EXPECT_EQ(*ctx.systemPrompt, "be helpful");
    ASSERT_TRUE(ctx.history.has_value());
    ASSERT_EQ(ctx.history->size(), 1);
    EXPECT_EQ(ctx.history->first().role, "user");
    ASSERT_EQ(ctx.history->first().blocks.size(), 1);
    EXPECT_EQ(ctx.history->first().blocks.first().kind, ContentBlockEntry::Kind::Text);
    EXPECT_EQ(ctx.history->first().blocks.first().text, "hello");
    EXPECT_EQ(manifest.historyMessages, 1);
    EXPECT_EQ(manifest.wireMessages, 1);
    EXPECT_EQ(manifest.systemChars, 10);
    EXPECT_EQ(manifest.textChars, 5);
}

TEST(ContextAssemblerTest, EmptySystemPromptIsOmitted)
{
    std::vector<Message> history;
    history.push_back(textMessage(Message::Role::User, "hi"));

    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr);

    EXPECT_FALSE(ctx.systemPrompt.has_value());
}

TEST(ContextAssemblerTest, SystemRoleMessagesAreSkipped)
{
    std::vector<Message> history;
    history.push_back(textMessage(Message::Role::System, "legacy system"));
    history.push_back(textMessage(Message::Role::User, "hi"));

    ContextAssembler::Manifest manifest;
    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr, {}, &manifest);

    ASSERT_TRUE(ctx.history.has_value());
    EXPECT_EQ(ctx.history->size(), 1);
    EXPECT_EQ(ctx.history->first().role, "user");
    EXPECT_FALSE(manifest.elided.isEmpty());
}

TEST(ContextAssemblerTest, CompletionContentBecomesPrefixSuffix)
{
    std::vector<Message> history;
    Message m(Message::Role::User);
    m.appendBlock(std::make_unique<CompletionContent>("int ma", "()\n"));
    history.push_back(std::move(m));

    ContextAssembler::Manifest manifest;
    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr, {}, &manifest);

    ASSERT_TRUE(ctx.prefix.has_value());
    EXPECT_EQ(*ctx.prefix, "int ma");
    ASSERT_TRUE(ctx.suffix.has_value());
    EXPECT_EQ(*ctx.suffix, "()\n");
    EXPECT_FALSE(ctx.history.has_value());
    EXPECT_TRUE(manifest.hasCompletionContext);
}

TEST(ContextAssemblerTest, UnsignedThinkingDroppedSignedKept)
{
    std::vector<Message> history;
    Message m(Message::Role::Assistant);
    m.appendBlock(std::make_unique<LLMQore::ThinkingContent>("draft", QString()));
    m.appendBlock(std::make_unique<LLMQore::ThinkingContent>("signed", "sig"));
    m.appendBlock(std::make_unique<LLMQore::TextContent>("answer"));
    history.push_back(std::move(m));

    ContextAssembler::Manifest manifest;
    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr, {}, &manifest);

    ASSERT_TRUE(ctx.history.has_value());
    const auto &blocks = ctx.history->first().blocks;
    ASSERT_EQ(blocks.size(), 2);
    EXPECT_EQ(blocks[0].kind, ContentBlockEntry::Kind::Thinking);
    EXPECT_EQ(blocks[0].signature, "sig");
    EXPECT_EQ(blocks[1].kind, ContentBlockEntry::Kind::Text);
    EXPECT_EQ(manifest.elided.size(), 1);
}

TEST(ContextAssemblerTest, ThinkingOnlyMessageIsDropped)
{
    std::vector<Message> history;
    Message m(Message::Role::Assistant);
    m.appendBlock(std::make_unique<LLMQore::ThinkingContent>("signed", "sig"));
    history.push_back(std::move(m));
    history.push_back(textMessage(Message::Role::User, "hi"));

    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr);

    ASSERT_TRUE(ctx.history.has_value());
    EXPECT_EQ(ctx.history->size(), 1);
    EXPECT_EQ(ctx.history->first().role, "user");
}

TEST(ContextAssemblerTest, OrphanToolUseIsDropped)
{
    std::vector<Message> history;
    Message m(Message::Role::Assistant);
    m.appendBlock(std::make_unique<LLMQore::TextContent>("calling"));
    m.appendBlock(
        std::make_unique<LLMQore::ToolUseContent>("tu1", "read_file", QJsonObject()));
    history.push_back(std::move(m));

    ContextAssembler::Manifest manifest;
    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr, {}, &manifest);

    ASSERT_TRUE(ctx.history.has_value());
    ASSERT_EQ(ctx.history->first().blocks.size(), 1);
    EXPECT_EQ(ctx.history->first().blocks.first().kind, ContentBlockEntry::Kind::Text);
    EXPECT_EQ(manifest.toolUseBlocks, 0);
    EXPECT_EQ(manifest.elided.size(), 1);
}

TEST(ContextAssemblerTest, OrphanToolResultIsDropped)
{
    std::vector<Message> history;
    Message m(Message::Role::User);
    m.appendBlock(std::make_unique<LLMQore::ToolResultContent>("unknown", "data"));
    history.push_back(std::move(m));
    history.push_back(textMessage(Message::Role::User, "hi"));

    ContextAssembler::Manifest manifest;
    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr, {}, &manifest);

    ASSERT_TRUE(ctx.history.has_value());
    EXPECT_EQ(ctx.history->size(), 1);
    EXPECT_EQ(manifest.toolResultBlocks, 0);
}

TEST(ContextAssemblerTest, PairedToolUseAndResultAreKept)
{
    std::vector<Message> history;
    Message use(Message::Role::Assistant);
    use.appendBlock(std::make_unique<LLMQore::ToolUseContent>(
        "tu1", "read_file", QJsonObject{{"path", "a.cpp"}}));
    history.push_back(std::move(use));
    Message result(Message::Role::User);
    result.appendBlock(std::make_unique<LLMQore::ToolResultContent>("tu1", "contents"));
    history.push_back(std::move(result));

    ContextAssembler::Manifest manifest;
    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr, {}, &manifest);

    ASSERT_TRUE(ctx.history.has_value());
    ASSERT_EQ(ctx.history->size(), 2);
    EXPECT_EQ(ctx.history->at(0).blocks.first().kind, ContentBlockEntry::Kind::ToolUse);
    EXPECT_EQ(ctx.history->at(1).blocks.first().kind, ContentBlockEntry::Kind::ToolResult);
    EXPECT_EQ(manifest.toolUseBlocks, 1);
    EXPECT_EQ(manifest.toolResultBlocks, 1);
    EXPECT_TRUE(manifest.elided.isEmpty());
}

TEST(ContextAssemblerTest, AttachmentMaterializedThroughLoader)
{
    std::vector<Message> history;
    Message m(Message::Role::User);
    m.appendBlock(std::make_unique<StoredAttachmentContent>("notes.txt", "stored/notes"));
    history.push_back(std::move(m));

    const auto ctx
        = ContextAssembler::assemble(history, QString(), base64Loader("file body"));

    ASSERT_TRUE(ctx.history.has_value());
    const auto &block = ctx.history->first().blocks.first();
    EXPECT_EQ(block.kind, ContentBlockEntry::Kind::Text);
    EXPECT_EQ(block.text, "File: notes.txt\n```\nfile body\n```");
}

TEST(ContextAssemblerTest, MissingAttachmentGetsPlaceholder)
{
    std::vector<Message> history;
    Message m(Message::Role::User);
    m.appendBlock(std::make_unique<StoredAttachmentContent>("notes.txt", "stored/notes"));
    history.push_back(std::move(m));

    ContextAssembler::Manifest manifest;
    const auto ctx = ContextAssembler::assemble(history, QString(), emptyLoader(), {}, &manifest);

    ASSERT_TRUE(ctx.history.has_value());
    const auto &block = ctx.history->first().blocks.first();
    EXPECT_EQ(block.kind, ContentBlockEntry::Kind::Text);
    EXPECT_EQ(block.text, "[Attachment unavailable: notes.txt]");
    EXPECT_EQ(manifest.elided.size(), 1);
}

TEST(ContextAssemblerTest, StoredImageMaterializedThroughLoader)
{
    std::vector<Message> history;
    Message m(Message::Role::User);
    m.appendBlock(
        std::make_unique<StoredImageContent>("shot.png", "stored/shot", "image/png"));
    history.push_back(std::move(m));

    ContextAssembler::Manifest manifest;
    const auto ctx
        = ContextAssembler::assemble(history, QString(), base64Loader("png"), {}, &manifest);

    ASSERT_TRUE(ctx.history.has_value());
    const auto &block = ctx.history->first().blocks.first();
    EXPECT_EQ(block.kind, ContentBlockEntry::Kind::Image);
    EXPECT_EQ(block.mediaType, "image/png");
    EXPECT_FALSE(block.isImageUrl);
    EXPECT_EQ(manifest.imageBlocks, 1);
}

TEST(ContextAssemblerTest, MissingImageWithNullLoaderGetsPlaceholder)
{
    std::vector<Message> history;
    Message m(Message::Role::User);
    m.appendBlock(
        std::make_unique<StoredImageContent>("shot.png", "stored/shot", "image/png"));
    history.push_back(std::move(m));

    ContextAssembler::Manifest manifest;
    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr, {}, &manifest);

    ASSERT_TRUE(ctx.history.has_value());
    const auto &block = ctx.history->first().blocks.first();
    EXPECT_EQ(block.kind, ContentBlockEntry::Kind::Text);
    EXPECT_EQ(block.text, "[Image unavailable: shot.png]");
    EXPECT_EQ(manifest.imageBlocks, 0);
    EXPECT_EQ(manifest.elided.size(), 1);
}

TEST(ContextAssemblerTest, PinnedBlocksPrependedToLastUserMessage)
{
    std::vector<Message> history;
    history.push_back(textMessage(Message::Role::User, "first"));
    history.push_back(textMessage(Message::Role::Assistant, "reply"));
    history.push_back(textMessage(Message::Role::User, "second"));

    ContextAssembler::Manifest manifest;
    const QVector<ContextAssembler::PinnedBlock> pinned{
        {"chat.files", "Linked files for reference:\nFile: a.cpp"}};
    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr, pinned, &manifest);

    ASSERT_TRUE(ctx.history.has_value());
    ASSERT_EQ(ctx.history->size(), 3);
    EXPECT_EQ(ctx.history->at(0).blocks.size(), 1);
    const auto &last = ctx.history->at(2);
    EXPECT_EQ(last.role, "user");
    ASSERT_EQ(last.blocks.size(), 2);
    EXPECT_EQ(last.blocks[0].text, "Linked files for reference:\nFile: a.cpp");
    EXPECT_EQ(last.blocks[1].text, "second");
    EXPECT_EQ(manifest.pinnedBlocks, 1);
}

TEST(ContextAssemblerTest, PinnedSkipsTrailingAssistantMessage)
{
    std::vector<Message> history;
    history.push_back(textMessage(Message::Role::User, "ask"));
    history.push_back(textMessage(Message::Role::Assistant, "answer"));

    const QVector<ContextAssembler::PinnedBlock> pinned{{"chat.files", "files"}};
    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr, pinned);

    ASSERT_TRUE(ctx.history.has_value());
    ASSERT_EQ(ctx.history->size(), 2);
    const auto &user = ctx.history->at(0);
    ASSERT_EQ(user.blocks.size(), 2);
    EXPECT_EQ(user.blocks[0].text, "files");
    EXPECT_EQ(user.blocks[1].text, "ask");
}

TEST(ContextAssemblerTest, PinnedWithoutUserMessageCreatesSyntheticOne)
{
    std::vector<Message> history;

    const QVector<ContextAssembler::PinnedBlock> pinned{{"chat.files", "files"}};
    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr, pinned);

    ASSERT_TRUE(ctx.history.has_value());
    ASSERT_EQ(ctx.history->size(), 1);
    EXPECT_EQ(ctx.history->first().role, "user");
    ASSERT_EQ(ctx.history->first().blocks.size(), 1);
    EXPECT_EQ(ctx.history->first().blocks.first().text, "files");
}

TEST(ContextAssemblerTest, EmptyPinnedTextIsIgnored)
{
    std::vector<Message> history;
    history.push_back(textMessage(Message::Role::User, "hi"));

    ContextAssembler::Manifest manifest;
    const QVector<ContextAssembler::PinnedBlock> pinned{{"chat.files", QString()}};
    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr, pinned, &manifest);

    ASSERT_TRUE(ctx.history.has_value());
    EXPECT_EQ(ctx.history->first().blocks.size(), 1);
    EXPECT_EQ(manifest.pinnedBlocks, 0);
}

TEST(ContextAssemblerTest, PinnedAnchorsToTypedMessageNotToolResults)
{
    std::vector<Message> history;
    history.push_back(textMessage(Message::Role::User, "fix the bug"));
    Message use(Message::Role::Assistant);
    use.appendBlock(
        std::make_unique<LLMQore::ToolUseContent>("tu1", "edit_file", QJsonObject()));
    history.push_back(std::move(use));
    Message result(Message::Role::User);
    result.appendBlock(std::make_unique<LLMQore::ToolResultContent>("tu1", "edited"));
    history.push_back(std::move(result));

    const QVector<ContextAssembler::PinnedBlock> pinned{{"chat.files", "files"}};
    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr, pinned);

    ASSERT_TRUE(ctx.history.has_value());
    ASSERT_EQ(ctx.history->size(), 3);
    const auto &typed = ctx.history->at(0);
    ASSERT_EQ(typed.blocks.size(), 2);
    EXPECT_EQ(typed.blocks[0].text, "files");
    EXPECT_EQ(typed.blocks[1].text, "fix the bug");
    EXPECT_EQ(ctx.history->at(2).blocks.size(), 1);
}

TEST(ContextAssemblerTest, PinnedInsertedAfterLeadingToolResults)
{
    std::vector<Message> history;
    Message use(Message::Role::Assistant);
    use.appendBlock(
        std::make_unique<LLMQore::ToolUseContent>("tu1", "edit_file", QJsonObject()));
    history.push_back(std::move(use));
    Message result(Message::Role::User);
    result.appendBlock(std::make_unique<LLMQore::ToolResultContent>("tu1", "edited"));
    history.push_back(std::move(result));

    const QVector<ContextAssembler::PinnedBlock> pinned{{"chat.files", "files"}};
    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr, pinned);

    ASSERT_TRUE(ctx.history.has_value());
    const auto &last = ctx.history->at(1);
    EXPECT_EQ(last.role, "user");
    ASSERT_EQ(last.blocks.size(), 2);
    EXPECT_EQ(last.blocks[0].kind, ContentBlockEntry::Kind::ToolResult);
    EXPECT_EQ(last.blocks[1].text, "files");
}

TEST(ContextAssemblerTest, SkillInvocationBecomesTextEntry)
{
    std::vector<Message> history;
    Message m(Message::Role::User);
    m.appendBlock(std::make_unique<LLMQore::TextContent>("/review this"));
    m.appendBlock(std::make_unique<SkillInvocationContent>("review", "Review the code."));
    history.push_back(std::move(m));

    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr);

    ASSERT_TRUE(ctx.history.has_value());
    const auto &blocks = ctx.history->first().blocks;
    ASSERT_EQ(blocks.size(), 2);
    EXPECT_EQ(blocks[1].kind, ContentBlockEntry::Kind::Text);
    EXPECT_EQ(blocks[1].text, "# Invoked Skill: review\n\nReview the code.");
}

TEST(ContextAssemblerTest, UnsupportedBlocksAreCounted)
{
    std::vector<Message> history;
    Message m(Message::Role::Assistant);
    m.appendBlock(std::make_unique<LLMQore::TextContent>("done"));
    m.appendBlock(std::make_unique<FileEditContent>("e1", "a.cpp", "old", "new"));
    history.push_back(std::move(m));

    ContextAssembler::Manifest manifest;
    const auto ctx = ContextAssembler::assemble(history, QString(), nullptr, {}, &manifest);

    ASSERT_TRUE(ctx.history.has_value());
    EXPECT_EQ(ctx.history->first().blocks.size(), 1);
    EXPECT_EQ(manifest.unsupportedBlocks, 1);
}
