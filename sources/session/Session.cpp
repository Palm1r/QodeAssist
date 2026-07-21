// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "session/Session.hpp"

#include <QJsonArray>
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

    if (m_backend) {
        cancel();
        disconnect(m_backend, nullptr, this, nullptr);
    }

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
    m_pendingPermissions.clear();
    m_alwaysAllowedToolKinds.clear();
    m_alwaysRejectedToolKinds.clear();
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
    const QList<ContentBlock> &userBlocks, const std::optional<TurnContext> &context)
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

    m_backend->sendTurn(request);
}

void Session::cancel()
{
    endTurn();

    if (m_backend)
        m_backend->cancel();
}

bool Session::isPermissionPending(const QString &requestId) const
{
    return m_pendingPermissions.contains(requestId);
}

void Session::respondPermission(const QString &requestId, const QString &optionId)
{
    if (!isPermissionPending(requestId)) {
        qWarning("QodeAssist: ignoring an answer to permission request %s, which is not pending",
                 qUtf8Printable(requestId));
        return;
    }

    const std::optional<PermissionBlock> block = permissionBlock(requestId);
    if (!block) {
        qWarning("QodeAssist: permission request %s has no record to answer",
                 qUtf8Printable(requestId));
        applyPermissionResolution(
            PermissionResolved{.turnId = m_activeTurnId, .requestId = requestId, .cancelled = true});
        return;
    }

    if (!block->option(optionId)) {
        qWarning("QodeAssist: permission request %s was not offered option %s",
                 qUtf8Printable(requestId), qUtf8Printable(optionId));
        return;
    }

    if (m_backend && m_backend->respondPermission(requestId, optionId))
        return;

    qWarning("QodeAssist: no backend accepted the answer to permission request %s",
             qUtf8Printable(requestId));
    applyPermissionResolution(
        PermissionResolved{.turnId = m_activeTurnId, .requestId = requestId, .cancelled = true});
}

std::optional<PermissionBlock> Session::permissionBlock(const QString &requestId) const
{
    const QList<Message> &messages = m_history.messages();

    for (auto message = messages.crbegin(); message != messages.crend(); ++message) {
        for (auto block = message->blocks.crbegin(); block != message->blocks.crend(); ++block) {
            if (const auto *permission = std::get_if<PermissionBlock>(&*block);
                permission && permission->requestId == requestId) {
                return *permission;
            }
        }
    }

    return std::nullopt;
}

QString Session::autoAnswerOptionFor(const PermissionRequested &request) const
{
    if (request.toolKind.isEmpty())
        return {};

    const bool rejected = m_alwaysRejectedToolKinds.contains(request.toolKind);
    if (!rejected && !m_alwaysAllowedToolKinds.contains(request.toolKind))
        return {};

    const QLatin1StringView once = rejected ? PermissionOptionKind::RejectOnce
                                            : PermissionOptionKind::AllowOnce;
    const QLatin1StringView always = rejected ? PermissionOptionKind::RejectAlways
                                              : PermissionOptionKind::AllowAlways;

    QString fallback;
    for (const PermissionOption &option : request.options) {
        if (option.kind == once)
            return option.id;
        if (option.kind == always && fallback.isEmpty())
            fallback = option.id;
    }

    return fallback;
}

bool Session::refreshAssistantBlockRow(
    const ContentBlock &block, RowKind kind, const QString &rowId)
{
    const Message *assistant = activeAssistantMessage();
    if (!assistant || m_assistantRowStart < 0)
        return false;

    auto fresh = projectBlockToRow(*assistant, block);
    if (!fresh)
        return false;

    for (int i = static_cast<int>(m_rows.size()) - 1; i >= m_assistantRowStart; --i) {
        if (m_rows[i].kind != kind || m_rows[i].id != rowId)
            continue;

        fresh->usage = m_rows.at(i).usage;
        if (m_rows.at(i) == *fresh)
            return true;

        m_rows[i] = *fresh;
        emit rowUpdated(i, *fresh);
        return true;
    }

    return false;
}

void Session::applyToolCall(const ToolCallUpdated &update)
{
    ensureAssistantMessage();

    Message *assistant = activeAssistantMessage();
    if (!assistant)
        return;

    const auto hasFileEdit = [assistant](const QString &editId) {
        for (const ContentBlock &block : assistant->blocks) {
            const auto *existing = std::get_if<FileEditBlock>(&block);
            if (existing && existing->id == editId)
                return true;
        }
        return false;
    };

    struct RecordedAgentEdit
    {
        QString editId;
        QString filePath;
        QString oldContent;
        QString newContent;
    };

    const auto appendAgentEdits =
        [assistant, &hasFileEdit](const ToolCallBlock &tool) -> QList<RecordedAgentEdit> {
        QList<RecordedAgentEdit> recorded;
        if (tool.status != QLatin1String("completed"))
            return recorded;

        const QJsonArray diffs = tool.details.value("diffs").toArray();
        for (qsizetype i = 0; i < diffs.size(); ++i) {
            const QJsonObject diff = diffs.at(i).toObject();
            const QString path = diff.value("path").toString();
            const QString oldText = diff.value("oldText").toString();
            const QString newText = diff.value("newText").toString();
            if (path.isEmpty() || (oldText.isEmpty() && newText.isEmpty()))
                continue;

            const QString editId = QString("agent-%1-%2").arg(tool.id).arg(i);
            if (hasFileEdit(editId))
                continue;

            const QJsonObject payload{
                {"edit_id", editId},
                {"file", path},
                {"old_content", oldText},
                {"new_content", newText},
                {"status", "applied"},
                {"status_message", "Applied by agent"}};
            assistant->blocks.append(
                FileEditBlock{.id = editId, .payload = encodeFileEditPayload(payload)});

            const bool revertable = !newText.isEmpty()
                                    && !diff.value("truncated").toBool(false);
            if (revertable)
                recorded.append({editId, path, oldText, newText});
        }
        return recorded;
    };

    for (auto it = assistant->blocks.rbegin(); it != assistant->blocks.rend(); ++it) {
        auto *tool = std::get_if<ToolCallBlock>(&*it);
        if (!tool || tool->id != update.toolId)
            continue;

        if (!update.name.isEmpty())
            tool->name = update.name;
        if (!update.kind.isEmpty())
            tool->kind = update.kind;
        if (!update.status.isEmpty())
            tool->status = update.status;
        if (!update.arguments.isEmpty())
            tool->arguments = update.arguments;
        if (!update.details.isEmpty())
            tool->details = update.details;
        if (!update.result.isEmpty())
            tool->result = update.result;

        const bool agentTool = tool->fromAgent;
        if (!agentTool) {
            if (const auto edit = fileEditFromToolResult(update.result);
                edit && !hasFileEdit(edit->id)) {
                m_textSegment.clear();
                assistant->blocks.append(*edit);
                syncAssistantRows();
                return;
            }
        } else {
            const ToolCallBlock snapshot = *tool;
            const qsizetype blocksBefore = assistant->blocks.size();
            const QList<RecordedAgentEdit> recorded = appendAgentEdits(snapshot);
            if (assistant->blocks.size() != blocksBefore) {
                m_textSegment.clear();
                syncAssistantRows();
                for (const RecordedAgentEdit &edit : recorded) {
                    emit agentFileEditRecorded(
                        m_activeTurnId,
                        edit.editId,
                        edit.filePath,
                        edit.oldContent,
                        edit.newContent);
                }
                return;
            }
        }

        const RowKind rowKind = agentTool ? RowKind::AgentTool : RowKind::Tool;
        const QString rowId = update.toolId;
        if (!refreshAssistantBlockRow(*it, rowKind, rowId))
            syncAssistantRows();
        return;
    }

    m_textSegment.clear();

    if (update.dropPrecedingText && !assistant->blocks.isEmpty()
        && std::get_if<TextBlock>(&assistant->blocks.last())) {
        assistant->blocks.removeLast();
    }

    const ToolCallBlock appended{
        .id = update.toolId,
        .name = update.name,
        .arguments = update.arguments,
        .result = update.result,
        .kind = update.kind,
        .status = update.status,
        .details = update.details,
        .fromAgent = update.fromAgent};
    assistant->blocks.append(appended);

    QList<RecordedAgentEdit> recorded;
    if (update.fromAgent) {
        recorded = appendAgentEdits(appended);
    } else if (const auto edit = fileEditFromToolResult(update.result)) {
        assistant->blocks.append(*edit);
    }

    syncAssistantRows();

    for (const RecordedAgentEdit &edit : recorded) {
        emit agentFileEditRecorded(
            m_activeTurnId, edit.editId, edit.filePath, edit.oldContent, edit.newContent);
    }
}

void Session::applyAgentPlan(const PlanUpdated &plan)
{
    ensureAssistantMessage();

    Message *assistant = activeAssistantMessage();
    if (!assistant)
        return;

    for (ContentBlock &block : assistant->blocks) {
        auto *existing = std::get_if<PlanBlock>(&block);
        if (!existing)
            continue;

        existing->entries = plan.entries;
        if (!refreshAssistantBlockRow(block, RowKind::Plan, assistant->id))
            syncAssistantRows();
        return;
    }

    if (plan.entries.isEmpty())
        return;

    m_textSegment.clear();
    assistant->blocks.append(PlanBlock{plan.entries});
    syncAssistantRows();
}

void Session::applyPermissionRequest(const PermissionRequested &request)
{
    const QString autoOptionId = autoAnswerOptionFor(request);

    m_textSegment.clear();
    m_pendingPermissions.insert(request.requestId);

    mutateAssistant([&request, &autoOptionId](Message &message) {
        message.blocks.append(
            PermissionBlock{
                .requestId = request.requestId,
                .toolCallId = request.toolCallId,
                .title = request.title,
                .toolKind = request.toolKind,
                .options = request.options,
                .status = PermissionStatus::Pending,
                .selectedOptionId = {},
                .automatic = !autoOptionId.isEmpty()});
    });

    if (autoOptionId.isEmpty())
        return;

    if (m_backend)
        m_backend->respondPermission(request.requestId, autoOptionId);
    else
        applyPermissionResolution(
            PermissionResolved{
                .turnId = request.turnId, .requestId = request.requestId, .cancelled = true});
}

void Session::applyPermissionResolution(const PermissionResolved &resolution)
{
    if (!m_pendingPermissions.remove(resolution.requestId))
        return;

    QString grantedToolKind;
    bool grantAllows = false;

    mutatePermissionBlock(resolution.requestId, [&](PermissionBlock &block) {
        block.status = resolution.cancelled ? PermissionStatus::Cancelled
                                            : PermissionStatus::Answered;
        block.selectedOptionId = resolution.cancelled ? QString() : resolution.optionId;
        if (resolution.cancelled) {
            block.automatic = false;
            return;
        }

        const std::optional<PermissionOption> option = block.option(resolution.optionId);
        if (option && option->scopesToConversation() && !block.toolKind.isEmpty()) {
            grantedToolKind = block.toolKind;
            grantAllows = option->allows();
        }
    });

    if (grantedToolKind.isEmpty())
        return;

    if (grantAllows)
        m_alwaysAllowedToolKinds.insert(grantedToolKind);
    else
        m_alwaysRejectedToolKinds.insert(grantedToolKind);
}

void Session::mutatePermissionBlock(
    const QString &requestId, const std::function<void(PermissionBlock &)> &mutate)
{
    PermissionBlock *target = nullptr;

    m_history.visitBlocks([&](ContentBlock &block) {
        if (auto *permission = std::get_if<PermissionBlock>(&block);
            permission && permission->requestId == requestId) {
            target = permission;
        }
    });

    if (!target)
        return;

    mutate(*target);
    const QString encodedPayload = encodePermissionBlock(*target);

    for (int i = static_cast<int>(m_rows.size()) - 1; i >= 0; --i) {
        if (m_rows[i].kind != RowKind::Permission || m_rows[i].id != requestId)
            continue;
        m_rows[i].content = encodedPayload;
        const MessageRow updated = m_rows.at(i);
        emit rowUpdated(i, updated);
        return;
    }
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
        std::variant_size_v<SessionEvent> == 11,
        "SessionEvent gained an alternative; extend the dispatch below or it is silently dropped");

    if (const auto *started = std::get_if<TurnStarted>(&event)) {
        m_activeTurnId = started->turnId;
        m_assistantRowStart = -1;
        emit turnStarted(started->turnId);
        return;
    }

    if (const auto *resolution = std::get_if<PermissionResolved>(&event)) {
        applyPermissionResolution(*resolution);
        return;
    }

    if (const auto *info = std::get_if<SessionInfo>(&event)) {
        constexpr qsizetype maxSessionTitleLength = 60;
        const QString title = info->title.simplified().left(maxSessionTitleLength);
        if (!title.isEmpty())
            emit sessionInfoReceived(title);
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
    } else if (const auto *toolUpdate = std::get_if<ToolCallUpdated>(&event)) {
        applyToolCall(*toolUpdate);
    } else if (const auto *plan = std::get_if<PlanUpdated>(&event)) {
        applyAgentPlan(*plan);
    } else if (const auto *permission = std::get_if<PermissionRequested>(&event)) {
        applyPermissionRequest(*permission);
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
    const QStringList unanswered(m_pendingPermissions.cbegin(), m_pendingPermissions.cend());
    for (const QString &requestId : unanswered) {
        qWarning("QodeAssist: permission request %s outlived its turn and was declined",
                 qUtf8Printable(requestId));
        applyPermissionResolution(
            PermissionResolved{.turnId = m_activeTurnId, .requestId = requestId, .cancelled = true});
    }

    if (Message *assistant = activeAssistantMessage()) {
        bool interrupted = false;
        for (ContentBlock &block : assistant->blocks) {
            auto *tool = std::get_if<ToolCallBlock>(&block);
            if (!tool || isTerminalToolStatus(tool->status))
                continue;
            tool->status = QStringLiteral("interrupted");
            interrupted = true;
        }
        if (interrupted)
            syncAssistantRows();
    }

    m_activeTurnId.clear();
    m_textSegment.clear();
    m_assistantRowStart = -1;
}

} // namespace QodeAssist::Session
