// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ContextAssembler.hpp"

#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSet>

#include <algorithm>

#include "Message.hpp"
#include "PluginBlocks.hpp"

namespace QodeAssist::ContextAssembler {

namespace {

Q_LOGGING_CATEGORY(ctxLog, "qodeassist.context")

QString roleToWireString(Message::Role role)
{
    switch (role) {
    case Message::Role::System:
        return QStringLiteral("system");
    case Message::Role::User:
        return QStringLiteral("user");
    case Message::Role::Assistant:
        return QStringLiteral("assistant");
    }
    return QStringLiteral("user");
}

bool isReplayableThinking(const LLMQore::ThinkingContent *block)
{
    return !block->signature().isEmpty();
}

Templates::ContentBlockEntry makeTextEntry(const QString &text)
{
    Templates::ContentBlockEntry e;
    e.kind = Templates::ContentBlockEntry::Kind::Text;
    e.text = text;
    return e;
}

QString placeholderFor(const QString &what, const QString &fileName)
{
    return QStringLiteral("[%1 unavailable: %2]").arg(what, fileName);
}

} // namespace

QString Manifest::summary() const
{
    QString s = QStringLiteral(
                    "system=%1ch, history=%2 msgs -> %3 wire, text=%4ch, thinking=%5ch, "
                    "tools=%6 use/%7 result (%8ch), images=%9")
                    .arg(systemChars)
                    .arg(historyMessages)
                    .arg(wireMessages)
                    .arg(textChars)
                    .arg(thinkingChars)
                    .arg(toolUseBlocks)
                    .arg(toolResultBlocks)
                    .arg(toolChars)
                    .arg(imageBlocks);
    if (pinnedBlocks > 0)
        s += QStringLiteral(", pinned=%1 (%2ch)").arg(pinnedBlocks).arg(pinnedChars);
    if (hasCompletionContext)
        s += QStringLiteral(", fim");
    if (unsupportedBlocks > 0)
        s += QStringLiteral(", unsupported=%1").arg(unsupportedBlocks);
    if (!elided.isEmpty())
        s += QStringLiteral(", elided=%1 [%2]")
                 .arg(elided.size())
                 .arg(elided.join(QStringLiteral("; ")));
    return s;
}

Templates::ContextData assemble(
    const std::vector<Message> &history,
    const QString &systemPrompt,
    const ContentLoader &loader,
    const QVector<PinnedBlock> &pinned,
    Manifest *outManifest)
{
    using Templates::ContentBlockEntry;
    using Templates::ContextData;
    using WireMessage = Templates::Message;

    Manifest manifest;
    manifest.historyMessages = static_cast<int>(history.size());

    ContextData ctx;
    if (!systemPrompt.isEmpty()) {
        ctx.systemPrompt = systemPrompt;
        manifest.systemChars = systemPrompt.size();
    }

    QSet<QString> resolvedToolUseIds;
    QSet<QString> declaredToolUseIds;
    for (const auto &m : history) {
        for (const auto &blockPtr : m.blocks()) {
            if (auto *tr = dynamic_cast<LLMQore::ToolResultContent *>(blockPtr.get()))
                resolvedToolUseIds.insert(tr->toolUseId());
            if (auto *tu = dynamic_cast<LLMQore::ToolUseContent *>(blockPtr.get()))
                declaredToolUseIds.insert(tu->id());
        }
    }

    QVector<WireMessage> wireHistory;

    for (const auto &m : history) {
        if (m.role() == Message::Role::System) {
            manifest.elided << QStringLiteral("system message skipped");
            continue;
        }

        if (auto *cc = m.lastBlockOfType<CompletionContent>()) {
            ctx.prefix = cc->prefix();
            ctx.suffix = cc->suffix();
            manifest.hasCompletionContext = true;
            continue;
        }

        QVector<ContentBlockEntry> blockEntries;

        for (const auto &blockPtr : m.blocks()) {
            auto *block = blockPtr.get();
            if (!block)
                continue;

            if (auto *t = dynamic_cast<LLMQore::TextContent *>(block)) {
                blockEntries.append(makeTextEntry(t->text()));
                manifest.textChars += t->text().size();
            } else if (auto *img = dynamic_cast<LLMQore::ImageContent *>(block)) {
                ContentBlockEntry e;
                e.kind = ContentBlockEntry::Kind::Image;
                e.imageData = img->data();
                e.mediaType = img->mediaType();
                e.isImageUrl = (img->sourceType() == LLMQore::ImageContent::ImageSourceType::Url);
                blockEntries.append(std::move(e));
                ++manifest.imageBlocks;
            } else if (auto *si = dynamic_cast<StoredImageContent *>(block)) {
                const QString base64 = loader ? loader(si->storedPath()) : QString();
                if (base64.isEmpty()) {
                    blockEntries.append(
                        makeTextEntry(placeholderFor(QStringLiteral("Image"), si->fileName())));
                    manifest.elided << QStringLiteral("image unavailable: %1").arg(si->fileName());
                    qCWarning(ctxLog).noquote()
                        << "stored image unavailable, placeholder inserted:" << si->fileName();
                    continue;
                }
                ContentBlockEntry e;
                e.kind = ContentBlockEntry::Kind::Image;
                e.imageData = base64;
                e.mediaType = si->mediaType();
                e.isImageUrl = false;
                blockEntries.append(std::move(e));
                ++manifest.imageBlocks;
            } else if (auto *sa = dynamic_cast<StoredAttachmentContent *>(block)) {
                const QString stored = loader ? loader(sa->storedPath()) : QString();
                if (stored.isEmpty()) {
                    blockEntries.append(makeTextEntry(
                        placeholderFor(QStringLiteral("Attachment"), sa->fileName())));
                    manifest.elided
                        << QStringLiteral("attachment unavailable: %1").arg(sa->fileName());
                    qCWarning(ctxLog).noquote()
                        << "stored attachment unavailable, placeholder inserted:" << sa->fileName();
                    continue;
                }
                const QString text = QString::fromUtf8(QByteArray::fromBase64(stored.toUtf8()));
                blockEntries.append(makeTextEntry(
                    QStringLiteral("File: %1\n```\n%2\n```").arg(sa->fileName(), text)));
                manifest.textChars += text.size();
            } else if (auto *sk = dynamic_cast<SkillInvocationContent *>(block)) {
                blockEntries.append(makeTextEntry(
                    QStringLiteral("# Invoked Skill: %1\n\n%2").arg(sk->skillName(), sk->body())));
                manifest.textChars += sk->body().size();
            } else if (auto *th = dynamic_cast<LLMQore::ThinkingContent *>(block)) {
                if (!isReplayableThinking(th)) {
                    manifest.elided << QStringLiteral("unsigned thinking dropped");
                    continue;
                }
                ContentBlockEntry e;
                e.kind = ContentBlockEntry::Kind::Thinking;
                e.thinking = th->thinking();
                e.signature = th->signature();
                blockEntries.append(std::move(e));
                manifest.thinkingChars += th->thinking().size();
            } else if (auto *rth = dynamic_cast<LLMQore::RedactedThinkingContent *>(block)) {
                if (rth->signature().isEmpty()) {
                    manifest.elided << QStringLiteral("unsigned redacted thinking dropped");
                    continue;
                }
                ContentBlockEntry e;
                e.kind = ContentBlockEntry::Kind::RedactedThinking;
                e.signature = rth->signature();
                blockEntries.append(std::move(e));
            } else if (auto *tu = dynamic_cast<LLMQore::ToolUseContent *>(block)) {
                if (!resolvedToolUseIds.contains(tu->id())) {
                    manifest.elided << QStringLiteral("orphan tool_use dropped: %1").arg(tu->id());
                    continue;
                }
                ContentBlockEntry e;
                e.kind = ContentBlockEntry::Kind::ToolUse;
                e.toolUseId = tu->id();
                e.toolName = tu->name();
                e.toolInput = tu->input();
                blockEntries.append(std::move(e));
                ++manifest.toolUseBlocks;
                manifest.toolChars
                    += QJsonDocument(tu->input()).toJson(QJsonDocument::Compact).size();
            } else if (auto *tr = dynamic_cast<LLMQore::ToolResultContent *>(block)) {
                if (!declaredToolUseIds.contains(tr->toolUseId())) {
                    manifest.elided
                        << QStringLiteral("orphan tool_result dropped: %1").arg(tr->toolUseId());
                    continue;
                }
                ContentBlockEntry e;
                e.kind = ContentBlockEntry::Kind::ToolResult;
                e.toolUseId = tr->toolUseId();
                e.result = tr->result();
                blockEntries.append(std::move(e));
                ++manifest.toolResultBlocks;
                manifest.toolChars += tr->result().size();
            } else {
                ++manifest.unsupportedBlocks;
            }
        }

        if (blockEntries.isEmpty())
            continue;

        const bool hasNonThinking
            = std::any_of(blockEntries.begin(), blockEntries.end(), [](const ContentBlockEntry &e) {
                  return e.kind != ContentBlockEntry::Kind::Thinking
                         && e.kind != ContentBlockEntry::Kind::RedactedThinking;
              });
        if (!hasNonThinking) {
            manifest.elided << QStringLiteral("thinking-only message dropped");
            continue;
        }

        WireMessage wm;
        wm.role = roleToWireString(m.role());
        wm.blocks = std::move(blockEntries);
        wireHistory.append(std::move(wm));
    }

    QVector<ContentBlockEntry> pinnedEntries;
    for (const auto &p : pinned) {
        if (p.text.isEmpty())
            continue;
        pinnedEntries.append(makeTextEntry(p.text));
        ++manifest.pinnedBlocks;
        manifest.pinnedChars += p.text.size();
    }
    if (!pinnedEntries.isEmpty()) {
        int anchorIndex = -1;
        int toolCarrierIndex = -1;
        for (int i = wireHistory.size() - 1; i >= 0; --i) {
            if (wireHistory[i].role != QLatin1String("user"))
                continue;
            const auto &blocks = wireHistory[i].blocks;
            const bool carriesToolResults = !blocks.isEmpty()
                                            && blocks.first().kind
                                                   == ContentBlockEntry::Kind::ToolResult;
            if (!carriesToolResults) {
                anchorIndex = i;
                break;
            }
            if (toolCarrierIndex < 0)
                toolCarrierIndex = i;
        }
        if (anchorIndex < 0)
            anchorIndex = toolCarrierIndex;
        if (anchorIndex < 0) {
            WireMessage wm;
            wm.role = QStringLiteral("user");
            wireHistory.append(std::move(wm));
            anchorIndex = wireHistory.size() - 1;
        }
        auto &target = wireHistory[anchorIndex].blocks;
        qsizetype insertPos = 0;
        while (insertPos < target.size()
               && target[insertPos].kind == ContentBlockEntry::Kind::ToolResult) {
            ++insertPos;
        }
        for (qsizetype i = 0; i < pinnedEntries.size(); ++i)
            target.insert(insertPos + i, pinnedEntries[i]);
    }

    manifest.wireMessages = wireHistory.size();
    if (!wireHistory.isEmpty())
        ctx.history = std::move(wireHistory);

    qCDebug(ctxLog).noquote() << manifest.summary();

    if (outManifest)
        *outManifest = std::move(manifest);

    return ctx;
}

} // namespace QodeAssist::ContextAssembler
