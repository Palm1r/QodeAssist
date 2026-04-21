// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ContextData.hpp"
#include "MessagePart.hpp"

#include <QAbstractListModel>
#include <QJsonArray>
#include <QtQmlIntegration>

#include "context/ContentFile.hpp"

namespace QodeAssist::Chat {

class ChatModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int tokensThreshold READ tokensThreshold NOTIFY tokensThresholdChanged FINAL)
    QML_ELEMENT

public:
    enum ChatRole { System, User, Assistant, Tool, FileEdit, Thinking };
    Q_ENUM(ChatRole)

    enum Roles { RoleType = Qt::UserRole, Content, Attachments, IsRedacted, Images };
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
    };

    explicit ChatModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addMessage(
        const QString &content,
        ChatRole role,
        const QString &id,
        const QList<Context::ContentFile> &attachments = {},
        const QList<ImageAttachment> &images = {},
        bool isRedacted = false,
        const QString &signature = QString());
    Q_INVOKABLE void clear();
    Q_INVOKABLE QList<MessagePart> processMessageContent(const QString &content) const;

    QVector<Message> getChatHistory() const;
    QJsonArray prepareMessagesForRequest(const QString &systemPrompt) const;

    int tokensThreshold() const;

    QString currentModel() const;
    QString lastMessageId() const;

    Q_INVOKABLE void resetModelTo(int index);

    void addToolExecutionStatus(
        const QString &requestId, const QString &toolId, const QString &toolName);
    void updateToolResult(
        const QString &requestId,
        const QString &toolId,
        const QString &toolName,
        const QString &result);
    void addThinkingBlock(
        const QString &requestId, const QString &thinking, const QString &signature);
    void addRedactedThinkingBlock(const QString &requestId, const QString &signature);
    void updateMessageContent(const QString &messageId, const QString &newContent);
    
    void setLoadingFromHistory(bool loading);
    bool isLoadingFromHistory() const;
    
    void setChatFilePath(const QString &filePath);
    QString chatFilePath() const;

signals:
    void tokensThresholdChanged();
    void modelReseted();

private slots:
    void onFileEditApplied(const QString &editId);
    void onFileEditRejected(const QString &editId);
    void onFileEditArchived(const QString &editId);

private:
    void updateFileEditStatus(const QString &editId, const QString &status, const QString &statusMessage);
    
    QVector<Message> m_messages;
    bool m_loadingFromHistory = false;
    QString m_chatFilePath;
};

} // namespace QodeAssist::Chat
Q_DECLARE_METATYPE(QodeAssist::Chat::ChatModel::Message)
Q_DECLARE_METATYPE(QodeAssist::Chat::MessagePart)
