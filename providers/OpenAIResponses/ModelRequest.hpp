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

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVariant>
#include <optional>
#include <variant>

namespace QodeAssist::OpenAIResponses {

enum class Role { User, Assistant, System, Developer };

enum class MessageStatus { InProgress, Completed, Incomplete };

enum class ReasoningEffort { None, Minimal, Low, Medium, High };

enum class TextFormat { Text, JsonSchema, JsonObject };

struct InputText
{
    QString text;

    QJsonObject toJson() const 
    { 
        return QJsonObject{{"type", "input_text"}, {"text", text}}; 
    }
    
    bool isValid() const noexcept { return !text.isEmpty(); }
};

struct InputImage
{
    std::optional<QString> fileId;
    std::optional<QString> imageUrl;
    QString detail = "auto";

    QJsonObject toJson() const
    {
        QJsonObject obj{{"type", "input_image"}, {"detail", detail}};
        if (fileId)
            obj["file_id"] = *fileId;
        if (imageUrl)
            obj["image_url"] = *imageUrl;
        return obj;
    }
    
    bool isValid() const noexcept { return fileId.has_value() || imageUrl.has_value(); }
};

struct InputFile
{
    std::optional<QString> fileId;
    std::optional<QString> fileUrl;
    std::optional<QString> fileData;
    std::optional<QString> filename;

    QJsonObject toJson() const
    {
        QJsonObject obj{{"type", "input_file"}};
        if (fileId)
            obj["file_id"] = *fileId;
        if (fileUrl)
            obj["file_url"] = *fileUrl;
        if (fileData)
            obj["file_data"] = *fileData;
        if (filename)
            obj["filename"] = *filename;
        return obj;
    }
    
    bool isValid() const noexcept 
    { 
        return fileId.has_value() || fileUrl.has_value() || fileData.has_value(); 
    }
};

class MessageContent
{
public:
    MessageContent(QString text) : m_variant(std::move(text)) {}
    MessageContent(InputText text) : m_variant(std::move(text)) {}
    MessageContent(InputImage image) : m_variant(std::move(image)) {}
    MessageContent(InputFile file) : m_variant(std::move(file)) {}

    QJsonValue toJson() const
    {
        return std::visit([](const auto &content) -> QJsonValue {
            using T = std::decay_t<decltype(content)>;
            if constexpr (std::is_same_v<T, QString>) {
                return content;
            } else {
                return content.toJson();
            }
        }, m_variant);
    }
    
    bool isValid() const noexcept
    {
        return std::visit([](const auto &content) -> bool {
            using T = std::decay_t<decltype(content)>;
            if constexpr (std::is_same_v<T, QString>) {
                return !content.isEmpty();
            } else {
                return content.isValid();
            }
        }, m_variant);
    }

private:
    std::variant<QString, InputText, InputImage, InputFile> m_variant;
};

struct Message
{
    Role role;
    QList<MessageContent> content;
    std::optional<MessageStatus> status;

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["role"] = roleToString(role);

        if (content.size() == 1) {
            obj["content"] = content[0].toJson();
        } else {
            QJsonArray arr;
            for (const auto &c : content) {
                arr.append(c.toJson());
            }
            obj["content"] = arr;
        }

        if (status) {
            obj["status"] = statusToString(*status);
        }

        return obj;
    }
    
    bool isValid() const noexcept
    {
        if (content.isEmpty()) {
            return false;
        }
        
        for (const auto &c : content) {
            if (!c.isValid()) {
                return false;
            }
        }
        
        return true;
    }

    static QString roleToString(Role r) noexcept
    {
        switch (r) {
        case Role::User:
            return "user";
        case Role::Assistant:
            return "assistant";
        case Role::System:
            return "system";
        case Role::Developer:
            return "developer";
        }
        return "user";
    }

    static QString statusToString(MessageStatus s) noexcept
    {
        switch (s) {
        case MessageStatus::InProgress:
            return "in_progress";
        case MessageStatus::Completed:
            return "completed";
        case MessageStatus::Incomplete:
            return "incomplete";
        }
        return "in_progress";
    }
};

struct FunctionTool
{
    QString name;
    QJsonObject parameters;
    std::optional<QString> description;
    bool strict = true;

    QJsonObject toJson() const
    {
        QJsonObject obj{{"type", "function"},
                        {"name", name},
                        {"parameters", parameters},
                        {"strict", strict}};
        if (description)
            obj["description"] = *description;
        return obj;
    }
    
    bool isValid() const noexcept 
    { 
        return !name.isEmpty() && !parameters.isEmpty(); 
    }
};

struct FileSearchTool
{
    QStringList vectorStoreIds;
    std::optional<int> maxNumResults;
    std::optional<double> scoreThreshold;

    QJsonObject toJson() const
    {
        QJsonObject obj{{"type", "file_search"}};
        QJsonArray ids;
        for (const auto &id : vectorStoreIds) {
            ids.append(id);
        }
        obj["vector_store_ids"] = ids;

        if (maxNumResults)
            obj["max_num_results"] = *maxNumResults;
        if (scoreThreshold)
            obj["score_threshold"] = *scoreThreshold;
        return obj;
    }
    
    bool isValid() const noexcept 
    { 
        return !vectorStoreIds.isEmpty(); 
    }
};

struct WebSearchTool
{
    QString searchContextSize = "medium";

    QJsonObject toJson() const
    {
        return QJsonObject{{"type", "web_search"}, {"search_context_size", searchContextSize}};
    }
    
    bool isValid() const noexcept 
    { 
        return !searchContextSize.isEmpty(); 
    }
};

struct CodeInterpreterTool
{
    QString container;

    QJsonObject toJson() const
    {
        return QJsonObject{{"type", "code_interpreter"}, {"container", container}};
    }
    
    bool isValid() const noexcept 
    { 
        return !container.isEmpty(); 
    }
};

class Tool
{
public:
    Tool(FunctionTool tool) : m_variant(std::move(tool)) {}
    Tool(FileSearchTool tool) : m_variant(std::move(tool)) {}
    Tool(WebSearchTool tool) : m_variant(std::move(tool)) {}
    Tool(CodeInterpreterTool tool) : m_variant(std::move(tool)) {}

    QJsonObject toJson() const
    {
        return std::visit([](const auto &t) { return t.toJson(); }, m_variant);
    }
    
    bool isValid() const noexcept
    {
        return std::visit([](const auto &t) { return t.isValid(); }, m_variant);
    }

private:
    std::variant<FunctionTool, FileSearchTool, WebSearchTool, CodeInterpreterTool> m_variant;
};

struct TextFormatOptions
{
    TextFormat type = TextFormat::Text;
    std::optional<QString> name;
    std::optional<QJsonObject> schema;
    std::optional<QString> description;
    std::optional<bool> strict;

    QJsonObject toJson() const
    {
        QJsonObject obj;

        switch (type) {
        case TextFormat::Text:
            obj["type"] = "text";
            break;
        case TextFormat::JsonSchema:
            obj["type"] = "json_schema";
            if (name)
                obj["name"] = *name;
            if (schema)
                obj["schema"] = *schema;
            if (description)
                obj["description"] = *description;
            if (strict)
                obj["strict"] = *strict;
            break;
        case TextFormat::JsonObject:
            obj["type"] = "json_object";
            break;
        }

        return obj;
    }
    
    bool isValid() const noexcept
    {
        if (type == TextFormat::JsonSchema) {
            return name.has_value() && schema.has_value();
        }
        return true;
    }
};

} // namespace QodeAssist::OpenAIResponses

