/* 
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include <optional>
#include <variant>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace QodeAssist::OpenAIResponses {

enum class ResponseStatus { Completed, Failed, InProgress, Cancelled, Queued, Incomplete };

enum class ItemStatus { InProgress, Completed, Incomplete };

struct FileCitation
{
    QString fileId;
    QString filename;
    int index = 0;

    static FileCitation fromJson(const QJsonObject &obj)
    {
        return {obj["file_id"].toString(), obj["filename"].toString(), obj["index"].toInt()};
    }
    
    bool isValid() const noexcept { return !fileId.isEmpty(); }
};

struct UrlCitation
{
    QString url;
    QString title;
    int startIndex = 0;
    int endIndex = 0;

    static UrlCitation fromJson(const QJsonObject &obj)
    {
        return {
            obj["url"].toString(),
            obj["title"].toString(),
            obj["start_index"].toInt(),
            obj["end_index"].toInt()};
    }
    
    bool isValid() const noexcept { return !url.isEmpty(); }
};

struct OutputText
{
    QString text;
    QList<FileCitation> fileCitations;
    QList<UrlCitation> urlCitations;

    static OutputText fromJson(const QJsonObject &obj)
    {
        OutputText result;
        result.text = obj["text"].toString();

        if (obj.contains("annotations")) {
            const QJsonArray annotations = obj["annotations"].toArray();
            result.fileCitations.reserve(annotations.size());
            result.urlCitations.reserve(annotations.size());
            
            for (const auto &annValue : annotations) {
                const QJsonObject ann = annValue.toObject();
                const QString type = ann["type"].toString();
                if (type == "file_citation") {
                    result.fileCitations.append(FileCitation::fromJson(ann));
                } else if (type == "url_citation") {
                    result.urlCitations.append(UrlCitation::fromJson(ann));
                }
            }
        }

        return result;
    }
    
    bool isValid() const noexcept { return !text.isEmpty(); }
};

struct Refusal
{
    QString refusal;

    static Refusal fromJson(const QJsonObject &obj) 
    { 
        return {obj["refusal"].toString()}; 
    }
    
    bool isValid() const noexcept { return !refusal.isEmpty(); }
};

struct MessageOutput
{
    QString id;
    QString role;
    ItemStatus status = ItemStatus::InProgress;
    QList<OutputText> outputTexts;
    QList<Refusal> refusals;

    static MessageOutput fromJson(const QJsonObject &obj)
    {
        MessageOutput result;
        result.id = obj["id"].toString();
        result.role = obj["role"].toString();

        const QString statusStr = obj["status"].toString();
        if (statusStr == "in_progress")
            result.status = ItemStatus::InProgress;
        else if (statusStr == "completed")
            result.status = ItemStatus::Completed;
        else
            result.status = ItemStatus::Incomplete;

        if (obj.contains("content")) {
            const QJsonArray content = obj["content"].toArray();
            result.outputTexts.reserve(content.size());
            result.refusals.reserve(content.size());
            
            for (const auto &item : content) {
                const QJsonObject itemObj = item.toObject();
                const QString type = itemObj["type"].toString();

                if (type == "output_text") {
                    result.outputTexts.append(OutputText::fromJson(itemObj));
                } else if (type == "refusal") {
                    result.refusals.append(Refusal::fromJson(itemObj));
                }
            }
        }

        return result;
    }
    
    bool isValid() const noexcept { return !id.isEmpty(); }
    bool hasContent() const noexcept { return !outputTexts.isEmpty() || !refusals.isEmpty(); }
};

struct FunctionCall
{
    QString id;
    QString callId;
    QString name;
    QString arguments;
    ItemStatus status = ItemStatus::InProgress;

    static FunctionCall fromJson(const QJsonObject &obj)
    {
        FunctionCall result;
        result.id = obj["id"].toString();
        result.callId = obj["call_id"].toString();
        result.name = obj["name"].toString();
        result.arguments = obj["arguments"].toString();

        const QString statusStr = obj["status"].toString();
        if (statusStr == "in_progress")
            result.status = ItemStatus::InProgress;
        else if (statusStr == "completed")
            result.status = ItemStatus::Completed;
        else
            result.status = ItemStatus::Incomplete;

        return result;
    }
    
    bool isValid() const noexcept { return !id.isEmpty() && !callId.isEmpty() && !name.isEmpty(); }
};

struct ReasoningOutput
{
    QString id;
    ItemStatus status = ItemStatus::InProgress;
    QString summaryText;
    QString encryptedContent;
    QList<QString> contentTexts;

    static ReasoningOutput fromJson(const QJsonObject &obj)
    {
        ReasoningOutput result;
        result.id = obj["id"].toString();

        const QString statusStr = obj["status"].toString();
        if (statusStr == "in_progress")
            result.status = ItemStatus::InProgress;
        else if (statusStr == "completed")
            result.status = ItemStatus::Completed;
        else
            result.status = ItemStatus::Incomplete;

        if (obj.contains("summary")) {
            const QJsonArray summary = obj["summary"].toArray();
            for (const auto &item : summary) {
                const QJsonObject itemObj = item.toObject();
                if (itemObj["type"].toString() == "summary_text") {
                    result.summaryText = itemObj["text"].toString();
                    break;
                }
            }
        }

        if (obj.contains("content")) {
            const QJsonArray content = obj["content"].toArray();
            result.contentTexts.reserve(content.size());
            
            for (const auto &item : content) {
                const QJsonObject itemObj = item.toObject();
                if (itemObj["type"].toString() == "reasoning_text") {
                    result.contentTexts.append(itemObj["text"].toString());
                }
            }
        }

        if (obj.contains("encrypted_content")) {
            result.encryptedContent = obj["encrypted_content"].toString();
        }

        return result;
    }
    
    bool isValid() const noexcept { return !id.isEmpty(); }
    bool hasContent() const noexcept 
    { 
        return !summaryText.isEmpty() || !contentTexts.isEmpty() || !encryptedContent.isEmpty(); 
    }
};

struct FileSearchResult
{
    QString fileId;
    QString filename;
    QString text;
    double score = 0.0;

    static FileSearchResult fromJson(const QJsonObject &obj)
    {
        return {
            obj["file_id"].toString(),
            obj["filename"].toString(),
            obj["text"].toString(),
            obj["score"].toDouble()};
    }
    
    bool isValid() const noexcept { return !fileId.isEmpty(); }
};

struct FileSearchCall
{
    QString id;
    QString status;
    QStringList queries;
    QList<FileSearchResult> results;

    static FileSearchCall fromJson(const QJsonObject &obj)
    {
        FileSearchCall result;
        result.id = obj["id"].toString();
        result.status = obj["status"].toString();

        if (obj.contains("queries")) {
            const QJsonArray queries = obj["queries"].toArray();
            result.queries.reserve(queries.size());
            
            for (const auto &q : queries) {
                result.queries.append(q.toString());
            }
        }

        if (obj.contains("results")) {
            const QJsonArray results = obj["results"].toArray();
            result.results.reserve(results.size());
            
            for (const auto &r : results) {
                result.results.append(FileSearchResult::fromJson(r.toObject()));
            }
        }

        return result;
    }
    
    bool isValid() const noexcept { return !id.isEmpty(); }
};

struct CodeInterpreterOutput
{
    QString type;
    QString logs;
    QString imageUrl;

    static CodeInterpreterOutput fromJson(const QJsonObject &obj)
    {
        CodeInterpreterOutput result;
        result.type = obj["type"].toString();
        if (result.type == "logs") {
            result.logs = obj["logs"].toString();
        } else if (result.type == "image") {
            result.imageUrl = obj["url"].toString();
        }
        return result;
    }
    
    bool isValid() const noexcept 
    { 
        return !type.isEmpty() && (!logs.isEmpty() || !imageUrl.isEmpty()); 
    }
};

struct CodeInterpreterCall
{
    QString id;
    QString containerId;
    std::optional<QString> code;
    QString status;
    QList<CodeInterpreterOutput> outputs;

    static CodeInterpreterCall fromJson(const QJsonObject &obj)
    {
        CodeInterpreterCall result;
        result.id = obj["id"].toString();
        result.containerId = obj["container_id"].toString();
        result.status = obj["status"].toString();

        if (obj.contains("code") && !obj["code"].isNull()) {
            result.code = obj["code"].toString();
        }

        if (obj.contains("outputs")) {
            const QJsonArray outputs = obj["outputs"].toArray();
            result.outputs.reserve(outputs.size());
            
            for (const auto &o : outputs) {
                result.outputs.append(CodeInterpreterOutput::fromJson(o.toObject()));
            }
        }

        return result;
    }
    
    bool isValid() const noexcept { return !id.isEmpty() && !containerId.isEmpty(); }
};

class OutputItem
{
public:
    enum class Type { Message, FunctionCall, Reasoning, FileSearch, CodeInterpreter, Unknown };

    explicit OutputItem(const MessageOutput &msg)
        : m_type(Type::Message)
        , m_data(msg)
    {}
    explicit OutputItem(const FunctionCall &call)
        : m_type(Type::FunctionCall)
        , m_data(call)
    {}
    explicit OutputItem(const ReasoningOutput &reasoning)
        : m_type(Type::Reasoning)
        , m_data(reasoning)
    {}
    explicit OutputItem(const FileSearchCall &search)
        : m_type(Type::FileSearch)
        , m_data(search)
    {}
    explicit OutputItem(const CodeInterpreterCall &interpreter)
        : m_type(Type::CodeInterpreter)
        , m_data(interpreter)
    {}

    Type type() const { return m_type; }

    const MessageOutput *asMessage() const
    {
        return std::holds_alternative<MessageOutput>(m_data) ? &std::get<MessageOutput>(m_data)
                                                             : nullptr;
    }

    const FunctionCall *asFunctionCall() const
    {
        return std::holds_alternative<FunctionCall>(m_data) ? &std::get<FunctionCall>(m_data)
                                                            : nullptr;
    }

    const ReasoningOutput *asReasoning() const
    {
        return std::holds_alternative<ReasoningOutput>(m_data) ? &std::get<ReasoningOutput>(m_data)
                                                               : nullptr;
    }

    const FileSearchCall *asFileSearch() const
    {
        return std::holds_alternative<FileSearchCall>(m_data) ? &std::get<FileSearchCall>(m_data)
                                                              : nullptr;
    }

    const CodeInterpreterCall *asCodeInterpreter() const
    {
        return std::holds_alternative<CodeInterpreterCall>(m_data)
                   ? &std::get<CodeInterpreterCall>(m_data)
                   : nullptr;
    }

    static OutputItem fromJson(const QJsonObject &obj)
    {
        const QString type = obj["type"].toString();

        if (type == "message") {
            return OutputItem(MessageOutput::fromJson(obj));
        } else if (type == "function_call") {
            return OutputItem(FunctionCall::fromJson(obj));
        } else if (type == "reasoning") {
            return OutputItem(ReasoningOutput::fromJson(obj));
        } else if (type == "file_search_call") {
            return OutputItem(FileSearchCall::fromJson(obj));
        } else if (type == "code_interpreter_call") {
            return OutputItem(CodeInterpreterCall::fromJson(obj));
        }

        return OutputItem(MessageOutput{});
    }

private:
    Type m_type;
    std::variant<MessageOutput, FunctionCall, ReasoningOutput, FileSearchCall, CodeInterpreterCall>
        m_data;
};

struct Usage
{
    int inputTokens = 0;
    int outputTokens = 0;
    int totalTokens = 0;

    static Usage fromJson(const QJsonObject &obj)
    {
        return {
            obj["input_tokens"].toInt(), 
            obj["output_tokens"].toInt(), 
            obj["total_tokens"].toInt()
        };
    }
    
    bool isValid() const noexcept { return totalTokens > 0; }
};

struct ResponseError
{
    QString code;
    QString message;

    static ResponseError fromJson(const QJsonObject &obj)
    {
        return {obj["code"].toString(), obj["message"].toString()};
    }
    
    bool isValid() const noexcept { return !code.isEmpty() && !message.isEmpty(); }
};

struct Response
{
    QString id;
    qint64 createdAt = 0;
    QString model;
    ResponseStatus status = ResponseStatus::InProgress;
    QList<OutputItem> output;
    QString outputText;
    std::optional<Usage> usage;
    std::optional<ResponseError> error;
    std::optional<QString> conversationId;

    static Response fromJson(const QJsonObject &obj)
    {
        Response result;
        result.id = obj["id"].toString();
        result.createdAt = obj["created_at"].toInteger();
        result.model = obj["model"].toString();

        const QString statusStr = obj["status"].toString();
        if (statusStr == "completed")
            result.status = ResponseStatus::Completed;
        else if (statusStr == "failed")
            result.status = ResponseStatus::Failed;
        else if (statusStr == "in_progress")
            result.status = ResponseStatus::InProgress;
        else if (statusStr == "cancelled")
            result.status = ResponseStatus::Cancelled;
        else if (statusStr == "queued")
            result.status = ResponseStatus::Queued;
        else
            result.status = ResponseStatus::Incomplete;

        if (obj.contains("output")) {
            const QJsonArray output = obj["output"].toArray();
            result.output.reserve(output.size());
            
            for (const auto &item : output) {
                result.output.append(OutputItem::fromJson(item.toObject()));
            }
        }

        if (obj.contains("output_text")) {
            result.outputText = obj["output_text"].toString();
        }

        if (obj.contains("usage")) {
            result.usage = Usage::fromJson(obj["usage"].toObject());
        }

        if (obj.contains("error")) {
            result.error = ResponseError::fromJson(obj["error"].toObject());
        }

        if (obj.contains("conversation")) {
            const QJsonObject conv = obj["conversation"].toObject();
            result.conversationId = conv["id"].toString();
        }

        return result;
    }

    QString getAggregatedText() const
    {
        if (!outputText.isEmpty()) {
            return outputText;
        }

        QString aggregated;
        for (const auto &item : output) {
            if (const auto *msg = item.asMessage()) {
                for (const auto &text : msg->outputTexts) {
                    aggregated += text.text;
                }
            }
        }
        return aggregated;
    }
    
    bool isValid() const noexcept { return !id.isEmpty(); }
    bool hasError() const noexcept { return error.has_value(); }
    bool isCompleted() const noexcept { return status == ResponseStatus::Completed; }
    bool isFailed() const noexcept { return status == ResponseStatus::Failed; }
};

} // namespace QodeAssist::OpenAIResponses

