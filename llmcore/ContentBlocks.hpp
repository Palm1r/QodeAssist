/* 
 * Copyright (C) 2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace QodeAssist::LLMCore {

enum class MessageState { Building, Complete, RequiresToolExecution, Final };

enum class ProviderFormat { Claude, OpenAI };

class ContentBlock : public QObject
{
    Q_OBJECT
public:
    explicit ContentBlock(QObject *parent = nullptr)
        : QObject(parent)
    {}
    virtual ~ContentBlock() = default;
    virtual QString type() const = 0;
    virtual QJsonValue toJson(ProviderFormat format) const = 0;
};

class TextContent : public ContentBlock
{
    Q_OBJECT
public:
    explicit TextContent(const QString &text = QString())
        : ContentBlock()
        , m_text(text)
    {}

    QString type() const override { return "text"; }
    QString text() const { return m_text; }
    void appendText(const QString &text) { m_text += text; }
    void setText(const QString &text) { m_text = text; }

    QJsonValue toJson(ProviderFormat format) const override
    {
        Q_UNUSED(format);
        return QJsonObject{{"type", "text"}, {"text", m_text}};
    }

private:
    QString m_text;
};

class ToolUseContent : public ContentBlock
{
    Q_OBJECT
public:
    ToolUseContent(const QString &id, const QString &name, const QJsonObject &input = QJsonObject())
        : ContentBlock()
        , m_id(id)
        , m_name(name)
        , m_input(input)
    {}

    QString type() const override { return "tool_use"; }
    QString id() const { return m_id; }
    QString name() const { return m_name; }
    QJsonObject input() const { return m_input; }
    void setInput(const QJsonObject &input) { m_input = input; }

    QJsonValue toJson(ProviderFormat format) const override
    {
        if (format == ProviderFormat::Claude) {
            return QJsonObject{
                {"type", "tool_use"}, {"id", m_id}, {"name", m_name}, {"input", m_input}};
        } else { // OpenAI
            QJsonDocument doc(m_input);
            return QJsonObject{
                {"id", m_id},
                {"type", "function"},
                {"function",
                 QJsonObject{
                     {"name", m_name},
                     {"arguments", QString::fromUtf8(doc.toJson(QJsonDocument::Compact))}}}};
        }
    }

private:
    QString m_id;
    QString m_name;
    QJsonObject m_input;
};

class ToolResultContent : public ContentBlock
{
    Q_OBJECT
public:
    ToolResultContent(const QString &toolUseId, const QString &result)
        : ContentBlock()
        , m_toolUseId(toolUseId)
        , m_result(result)
    {}

    QString type() const override { return "tool_result"; }
    QString toolUseId() const { return m_toolUseId; }
    QString result() const { return m_result; }

    QJsonValue toJson(ProviderFormat format) const override
    {
        if (format == ProviderFormat::Claude) {
            return QJsonObject{
                {"type", "tool_result"}, {"tool_use_id", m_toolUseId}, {"content", m_result}};
        } else { // OpenAI
            return QJsonObject{{"role", "tool"}, {"tool_call_id", m_toolUseId}, {"content", m_result}};
        }
    }

private:
    QString m_toolUseId;
    QString m_result;
};

class ThinkingContent : public ContentBlock
{
    Q_OBJECT
public:
    explicit ThinkingContent(const QString &thinking = QString(), const QString &signature = QString())
        : ContentBlock()
        , m_thinking(thinking)
        , m_signature(signature)
    {}

    QString type() const override { return "thinking"; }
    QString thinking() const { return m_thinking; }
    QString signature() const { return m_signature; }
    void appendThinking(const QString &text) { m_thinking += text; }
    void setThinking(const QString &text) { m_thinking = text; }
    void setSignature(const QString &signature) { m_signature = signature; }

    QJsonValue toJson(ProviderFormat format) const override
    {
        Q_UNUSED(format);
        // Only include signature field if it's not empty
        // Empty signature is rejected by API with "Invalid signature" error
        // In streaming mode, signature is not provided, so we omit the field entirely
        QJsonObject obj{{"type", "thinking"}, {"thinking", m_thinking}};
        if (!m_signature.isEmpty()) {
            obj["signature"] = m_signature;
        }
        return obj;
    }

private:
    QString m_thinking;
    QString m_signature;
};

class RedactedThinkingContent : public ContentBlock
{
    Q_OBJECT
public:
    explicit RedactedThinkingContent(const QString &signature = QString())
        : ContentBlock()
        , m_signature(signature)
    {}

    QString type() const override { return "redacted_thinking"; }
    QString signature() const { return m_signature; }
    void setSignature(const QString &signature) { m_signature = signature; }

    QJsonValue toJson(ProviderFormat format) const override
    {
        Q_UNUSED(format);
        // Only include signature field if it's not empty
        // Empty signature is rejected by API with "Invalid signature" error
        QJsonObject obj{{"type", "redacted_thinking"}};
        if (!m_signature.isEmpty()) {
            obj["signature"] = m_signature;
        }
        return obj;
    }

private:
    QString m_signature;
};

} // namespace QodeAssist::LLMCore
