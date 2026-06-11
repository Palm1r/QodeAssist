// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include "ContextData.hpp"
#include "MessagePart.hpp"

#include <QAbstractListModel>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QVector>
#include <QtQmlIntegration>

namespace QodeAssist {
class ConversationHistory;
}

namespace QodeAssist::Chat {

class ChatModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int sessionPromptTokens READ sessionPromptTokens NOTIFY sessionUsageChanged FINAL)
    Q_PROPERTY(int sessionCompletionTokens READ sessionCompletionTokens NOTIFY sessionUsageChanged FINAL)
    Q_PROPERTY(int sessionCachedPromptTokens READ sessionCachedPromptTokens NOTIFY sessionUsageChanged FINAL)
    Q_PROPERTY(int sessionTotalTokens READ sessionTotalTokens NOTIFY sessionUsageChanged FINAL)
    QML_ELEMENT

public:
    enum ChatRole { System, User, Assistant, Tool, FileEdit, Thinking };
    Q_ENUM(ChatRole)

    enum Roles {
        RoleType = Qt::UserRole,
        Content,
        Attachments,
        IsRedacted,
        Images,
        PromptTokens,
        CompletionTokens,
        CachedPromptTokens,
        ReasoningTokens,
        TotalTokens
    };
    Q_ENUM(Roles)

    explicit ChatModel(QObject *parent = nullptr);

    void setHistory(ConversationHistory *history);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void clear();
    Q_INVOKABLE QList<MessagePart> processMessageContent(const QString &content) const;
    Q_INVOKABLE void resetModelTo(int index);
    Q_INVOKABLE QVariantList userMessagePreviews(int maxLength = 80) const;

    void setMessageUsage(
        const QString &messageId,
        int promptTokens,
        int completionTokens,
        int cachedPromptTokens,
        int reasoningTokens);

    int sessionPromptTokens() const;
    int sessionCompletionTokens() const;
    int sessionCachedPromptTokens() const;
    int sessionTotalTokens() const;

    void setChatFilePath(const QString &filePath);
    QString chatFilePath() const;

signals:
    void modelReseted();
    void sessionUsageChanged();

private slots:
    void onHistoryMessageAdded(int index);
    void onHistoryMessageUpdated(int index);
    void onHistoryCleared();
    void onHistoryReset();
    void onFileEditStatusChanged(const QString &editId);

private:
    struct AttachmentRef
    {
        QString fileName;
        QString storedPath;
    };
    struct ImageRef
    {
        QString fileName;
        QString storedPath;
        QString mediaType;
    };
    struct Row
    {
        ChatRole kind = ChatRole::Assistant;
        int messageIndex = -1;
        QString messageId;
        QString content;
        bool isRedacted = false;
        QString editId;
        QVector<AttachmentRef> attachments;
        QVector<ImageRef> images;
    };
    struct Usage
    {
        int prompt = 0;
        int completion = 0;
        int cached = 0;
        int reasoning = 0;
    };

    void rebuildAll();
    void reprojectTail(int startMessageIndex);
    int startMessageIndexFor(int messageIndex) const;
    int firstRowForMessage(int messageIndex) const;
    QHash<QString, QString> buildToolResultMap() const;
    void appendRowsForMessage(
        int messageIndex, const QHash<QString, QString> &toolResults, QVector<Row> &out) const;
    QString overlayFileEditStatus(const QString &content, const QString &editId) const;
    QVariantList buildAttachmentList(const QVector<AttachmentRef> &attachments) const;
    QVariantList buildImageList(const QVector<ImageRef> &images) const;

    QPointer<ConversationHistory> m_history;
    QVector<Row> m_rows;
    QHash<QString, Usage> m_usageByMessageId;
    QString m_chatFilePath;
};

} // namespace QodeAssist::Chat

Q_DECLARE_METATYPE(QodeAssist::Chat::MessagePart)
