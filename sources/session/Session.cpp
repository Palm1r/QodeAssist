// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/Session.hpp"

#include <QUuid>

#include <algorithm>

#include "session/FileEditPayload.hpp"

namespace QodeAssist::Session {

namespace {

std::optional<FileEditBlock> fileEditFromToolResult(const QString &result)
{
    if (!isFileEditPayload(result))
        return std::nullopt;

    auto payload = parseFileEditPayload(result);
    if (!payload)
        return std::nullopt;

    const QString editId = payload->value("edit_id").toString();
    if (!editId.isEmpty())
        return FileEditBlock{.id = editId, .payload = result};

    const QString generatedId = QString("edit_%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    payload->insert("edit_id", generatedId);

    return FileEditBlock{.id = generatedId, .payload = encodeFileEditPayload(*payload)};
}

Message truncateMessage(const Message &message, qsizetype rowsWanted)
{
    Message truncated = message;
    qsizetype keptBlocks = 0;

    for (qsizetype i = 0; i < message.blocks.size(); ++i) {
        Message probe = message;
        probe.blocks = message.blocks.mid(0, i + 1);
        if (projectMessageToRows(probe).size() > rowsWanted)
            break;
        keptBlocks = i + 1;
    }

    truncated.blocks = message.blocks.mid(0, keptBlocks);
    return truncated;
}

bool continuesThinking(const ThinkingBlock &block, const ThinkingReceived &event)
{
    return !block.redacted && !event.redacted && event.text.startsWith(block.text)
           && (block.signature.isEmpty() || block.signature == event.signature);
}

} // namespace

Session::Session(QObject *parent)
    : QObject(parent)
{}

void Session::setBackend(ChatBackend *backend)
{
    if (m_backend == backend)
        return;

    if (m_backend)
        disconnect(m_backend, nullptr, this, nullptr);

    m_backend = backend;

    if (m_backend) {
        connect(m_backend, &ChatBackend::sessionEvent, this, &Session::handleEvent);
    }
}

const ConversationHistory &Session::history() const
{
    return m_history;
}

const QList<MessageRow> &Session::rows() const
{
    return m_rows;
}

void Session::setHistory(const ConversationHistory &history)
{
    cancel();
    m_history = history;
    m_rows = projectToRows(m_history);
    emit rowsReset(m_rows);
}

void Session::clear()
{
    setHistory(ConversationHistory{});
}

void Session::truncateRows(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= m_rows.size())
        return;

    cancel();

    ConversationHistory kept;
    qsizetype rowsKept = 0;

    for (const Message &message : m_history.messages()) {
        const qsizetype rows = projectMessageToRows(message).size();
        if (rowsKept + rows > rowIndex) {
            const qsizetype rowsWanted = rowIndex - rowsKept;
            if (rowsWanted > 0)
                kept.append(truncateMessage(message, rowsWanted));
            break;
        }
        kept.append(message);
        rowsKept += rows;
    }

    const int removed = static_cast<int>(m_rows.size()) - rowIndex;
    m_rows.remove(rowIndex, removed);
    m_history = kept;

    emit rowsRemoved(rowIndex, removed);
}

void Session::sendTurn(
    const QList<ContentBlock> &userBlocks,
    const std::optional<TurnContext> &context,
    const TurnOptions &options)
{
    if (!m_backend)
        return;

    cancel();

    Message message;
    message.role = MessageRole::User;
    message.blocks = userBlocks;
    appendMessage(message);

    TurnRequest request;
    request.userBlocks = userBlocks;
    request.history = &m_history;
    request.context = context;
    request.options = options;

    m_backend->sendTurn(request);
}

void Session::cancel()
{
    endTurn();

    if (m_backend)
        m_backend->cancel();
}

bool Session::updateFileEditStatus(
    const QString &editId, const QString &status, const QString &statusMessage)
{
    QStringList encodedPayloads;

    m_history.visitBlocks([&](ContentBlock &block) {
        auto *edit = std::get_if<FileEditBlock>(&block);
        if (!edit || edit->id != editId)
            return;

        auto payload = parseFileEditPayload(edit->payload);
        if (!payload)
            return;

        payload->insert("status", status);
        if (!statusMessage.isEmpty())
            payload->insert("status_message", statusMessage);

        edit->payload = encodeFileEditPayload(*payload);
        encodedPayloads.append(edit->payload);
    });

    if (encodedPayloads.isEmpty())
        return false;

    qsizetype matched = 0;
    for (int i = 0; i < m_rows.size() && matched < encodedPayloads.size(); ++i) {
        if (m_rows[i].kind != RowKind::FileEdit || m_rows[i].id != editId)
            continue;
        m_rows[i].content = encodedPayloads.at(matched++);
        const MessageRow updated = m_rows.at(i);
        emit rowUpdated(i, updated);
    }

    return true;
}

void Session::handleEvent(const SessionEvent &event)
{
    static_assert(
        std::variant_size_v<SessionEvent> == 8,
        "SessionEvent gained an alternative; extend the dispatch below or it is silently dropped");

    if (const auto *started = std::get_if<TurnStarted>(&event)) {
        m_activeTurnId = started->turnId;
        m_assistantRowStart = -1;
        emit turnStarted(started->turnId);
        return;
    }

    if (const auto *failure = std::get_if<TurnFailed>(&event)) {
        if (!failure->turnId.isEmpty() && failure->turnId != m_activeTurnId)
            return;
        const QString error = failure->error;
        endTurn();
        emit turnFailed(error);
        return;
    }

    if (m_activeTurnId.isEmpty() || turnIdOf(event) != m_activeTurnId)
        return;

    if (const auto *delta = std::get_if<TextDelta>(&event)) {
        m_textSegment += delta->text;

        if (!m_textSegment.isEmpty() && m_textSegment.at(0).isSpace()) {
            qsizetype content = 0;
            while (content < m_textSegment.size() && m_textSegment.at(content).isSpace())
                ++content;
            m_textSegment.remove(0, content);
        }

        if (m_textSegment.isEmpty())
            return;

        const QString text = m_textSegment.trimmed();

        mutateAssistantTail([&text](Message &message) {
            if (!message.blocks.isEmpty()) {
                if (auto *last = std::get_if<TextBlock>(&message.blocks.last())) {
                    last->text = text;
                    return;
                }
            }
            message.blocks.append(TextBlock{text});
        });
    } else if (const auto *thinking = std::get_if<ThinkingReceived>(&event)) {
        m_textSegment.clear();
        mutateAssistantTail([thinking](Message &message) {
            if (!message.blocks.isEmpty()) {
                if (auto *last = std::get_if<ThinkingBlock>(&message.blocks.last());
                    last && continuesThinking(*last, *thinking)) {
                    last->text = thinking->text;
                    last->signature = thinking->signature;
                    return;
                }
            }
            message.blocks.append(
                ThinkingBlock{
                    .text = thinking->text,
                    .signature = thinking->signature,
                    .redacted = thinking->redacted});
        });
    } else if (const auto *toolStart = std::get_if<ToolCallStarted>(&event)) {
        m_textSegment.clear();
        mutateAssistant([toolStart](Message &message) {
            if (toolStart->dropPrecedingText && !message.blocks.isEmpty()
                && std::holds_alternative<TextBlock>(message.blocks.last())) {
                message.blocks.removeLast();
            }
            message.blocks.append(
                ToolCallBlock{
                    .id = toolStart->toolId,
                    .name = toolStart->name,
                    .arguments = toolStart->arguments,
                    .result = {}});
        });
    } else if (const auto *toolEnd = std::get_if<ToolCallCompleted>(&event)) {
        m_textSegment.clear();
        mutateAssistant([toolEnd](Message &message) {
            for (auto it = message.blocks.rbegin(); it != message.blocks.rend(); ++it) {
                auto *tool = std::get_if<ToolCallBlock>(&*it);
                if (!tool || tool->id != toolEnd->toolId)
                    continue;
                tool->name = toolEnd->name;
                tool->result = toolEnd->result;
                break;
            }
            if (const auto edit = fileEditFromToolResult(toolEnd->result))
                message.blocks.append(*edit);
        });
    } else if (const auto *usage = std::get_if<UsageReported>(&event)) {
        if (Message *assistant = activeAssistantMessage()) {
            assistant->usage = usage->usage;
            syncAssistantRows();
        }
        emit usageReceived(usage->usage);
    } else if (const auto *completed = std::get_if<TurnCompleted>(&event)) {
        const QString turnId = completed->turnId;
        endTurn();
        emit turnFinished(turnId);
    }
}

void Session::appendMessage(const Message &message)
{
    m_history.append(message);

    const QList<MessageRow> rows = projectMessageToRows(message);
    if (rows.isEmpty())
        return;

    m_rows.append(rows);
    emit rowsAppended(rows);
}

void Session::ensureAssistantMessage()
{
    if (m_assistantRowStart >= 0)
        return;

    Message message;
    message.role = MessageRole::Assistant;
    message.id = m_activeTurnId;
    m_assistantRowStart = static_cast<int>(m_rows.size());
    m_history.append(message);
}

Message *Session::activeAssistantMessage()
{
    return m_assistantRowStart < 0 ? nullptr : m_history.lastMessage();
}

void Session::mutateAssistant(const std::function<void(Message &)> &mutate)
{
    ensureAssistantMessage();

    Message *assistant = activeAssistantMessage();
    if (!assistant)
        return;

    mutate(*assistant);
    syncAssistantRows();
}

void Session::mutateAssistantTail(const std::function<void(Message &)> &mutate)
{
    ensureAssistantMessage();

    Message *assistant = activeAssistantMessage();
    if (!assistant)
        return;

    const qsizetype blocksBefore = assistant->blocks.size();
    mutate(*assistant);

    if (assistant->blocks.size() == blocksBefore)
        updateLastAssistantRow();
    else
        syncAssistantRows();
}

void Session::updateLastAssistantRow()
{
    const Message *assistant = activeAssistantMessage();
    const int index = static_cast<int>(m_rows.size()) - 1;
    if (!assistant || index < m_assistantRowStart || assistant->blocks.isEmpty()) {
        syncAssistantRows();
        return;
    }

    auto fresh = projectBlockToRow(*assistant, assistant->blocks.last());
    if (!fresh) {
        syncAssistantRows();
        return;
    }

    fresh->usage = m_rows.at(index).usage;
    if (m_rows.at(index) == *fresh)
        return;

    const MessageRow updated = *fresh;
    m_rows[index] = updated;
    emit rowUpdated(index, updated);
}

void Session::syncAssistantRows()
{
    const Message *assistant = activeAssistantMessage();
    if (!assistant)
        return;

    const QList<MessageRow> fresh = projectMessageToRows(*assistant);
    const int start = m_assistantRowStart;
    const int previous = static_cast<int>(m_rows.size()) - start;

    const int common = std::min(previous, static_cast<int>(fresh.size()));
    for (int i = 0; i < common; ++i) {
        if (m_rows.at(start + i) == fresh.at(i))
            continue;
        m_rows[start + i] = fresh[i];
        emit rowUpdated(start + i, fresh[i]);
    }

    if (fresh.size() > previous) {
        const QList<MessageRow> added = fresh.mid(previous);
        m_rows.append(added);
        emit rowsAppended(added);
    } else if (fresh.size() < previous) {
        const int removed = previous - static_cast<int>(fresh.size());
        m_rows.remove(start + fresh.size(), removed);
        emit rowsRemoved(start + static_cast<int>(fresh.size()), removed);
    }
}

void Session::endTurn()
{
    m_activeTurnId.clear();
    m_textSegment.clear();
    m_assistantRowStart = -1;
}

} // namespace QodeAssist::Session
