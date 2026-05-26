// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <LLMQore/ContentBlocks.hpp>

#include <QString>

namespace QodeAssist {

class StoredImageContent : public LLMQore::ContentBlock
{
public:
    StoredImageContent(QString fileName, QString storedPath, QString mediaType)
        : m_fileName(std::move(fileName))
        , m_storedPath(std::move(storedPath))
        , m_mediaType(std::move(mediaType))
    {}

    QString type() const override { return QStringLiteral("stored_image"); }

    QString fileName() const { return m_fileName; }
    QString storedPath() const { return m_storedPath; }
    QString mediaType() const { return m_mediaType; }

private:
    QString m_fileName;
    QString m_storedPath;
    QString m_mediaType;
};

class StoredAttachmentContent : public LLMQore::ContentBlock
{
public:
    StoredAttachmentContent(QString fileName, QString storedPath)
        : m_fileName(std::move(fileName))
        , m_storedPath(std::move(storedPath))
    {}

    QString type() const override { return QStringLiteral("stored_attachment"); }

    QString fileName() const { return m_fileName; }
    QString storedPath() const { return m_storedPath; }

private:
    QString m_fileName;
    QString m_storedPath;
};

class FileEditContent : public LLMQore::ContentBlock
{
public:
    enum class Status { Pending, Applied, Rejected, Archived };

    FileEditContent(
        QString editId,
        QString filePath,
        QString oldContent,
        QString newContent,
        Status status = Status::Pending,
        QString statusMessage = QString())
        : m_editId(std::move(editId))
        , m_filePath(std::move(filePath))
        , m_oldContent(std::move(oldContent))
        , m_newContent(std::move(newContent))
        , m_status(status)
        , m_statusMessage(std::move(statusMessage))
    {}

    QString type() const override { return QStringLiteral("file_edit"); }

    QString editId() const { return m_editId; }
    QString filePath() const { return m_filePath; }
    QString oldContent() const { return m_oldContent; }
    QString newContent() const { return m_newContent; }
    Status status() const { return m_status; }
    QString statusMessage() const { return m_statusMessage; }

    void setStatus(Status status) { m_status = status; }
    void setStatusMessage(QString msg) { m_statusMessage = std::move(msg); }

    static QString statusToString(Status s)
    {
        switch (s) {
        case Status::Pending: return QStringLiteral("pending");
        case Status::Applied: return QStringLiteral("applied");
        case Status::Rejected: return QStringLiteral("rejected");
        case Status::Archived: return QStringLiteral("archived");
        }
        return QStringLiteral("pending");
    }

    static Status statusFromString(const QString &s)
    {
        if (s == QLatin1String("applied"))
            return Status::Applied;
        if (s == QLatin1String("rejected"))
            return Status::Rejected;
        if (s == QLatin1String("archived"))
            return Status::Archived;
        return Status::Pending;
    }

private:
    QString m_editId;
    QString m_filePath;
    QString m_oldContent;
    QString m_newContent;
    Status m_status;
    QString m_statusMessage;
};

} // namespace QodeAssist
