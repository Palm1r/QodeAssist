// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include "MessagePart.hpp"

#include <QAbstractListModel>
#include <QJsonObject>
#include <QtQmlIntegration>

#include "context/ContentFile.hpp"

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
    enum ChatRole { System, User, Assistant, Tool, FileEdit, Thinking, Permission, Plan };
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
        TotalTokens,
        ToolKind,
        ToolStatus,
        ToolDetails
    };
    Q_ENUM(Roles)

    struct ImageAttachment
    {
        QString fileName;      // Original filename
        QString storedPath;    // Path to stored image file (relative to chat folder)
        QString mediaType;     // MIME type
    };

    struct Message
    {
        ChatRole role;
        QString content;
        QString id;
        bool isRedacted = false;
        QString signature = QString();

        QList<Context::ContentFile> attachments;
        QList<ImageAttachment> images;

        QString toolName;
        QJsonObject toolArguments;
        QString toolResult;
        QString toolKind;
        QString toolStatus;
        QVariantMap toolDetails;

        int promptTokens = 0;
        int completionTokens = 0;
        int cachedPromptTokens = 0;
        int reasoningTokens = 0;
    };

    explicit ChatModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void resetMessages(const QVector<Message> &messages);
    void appendMessages(const QVector<Message> &messages);
    void updateMessage(int index, const Message &message);
    void removeMessages(int first, int count);

    Q_INVOKABLE QList<MessagePart> processMessageContent(const QString &content) const;
    Q_INVOKABLE QVariantList userMessagePreviews(int maxLength = 80) const;

    int sessionPromptTokens() const;
    int sessionCompletionTokens() const;
    int sessionCachedPromptTokens() const;
    int sessionTotalTokens() const;

    void setChatFilePath(const QString &filePath);
    QString chatFilePath() const;

signals:
    void modelReseted();
    void sessionUsageChanged();

private:
    QVector<Message> m_messages;
    QString m_chatFilePath;
};

} // namespace QodeAssist::Chat
Q_DECLARE_METATYPE(QodeAssist::Chat::ChatModel::Message)
Q_DECLARE_METATYPE(QodeAssist::Chat::MessagePart)
