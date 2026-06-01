// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "JsonPromptTemplate.hpp"

#include <QDebug>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>

#include <filesystem>

#include "AgentConfig.hpp"

namespace QodeAssist::Templates {

namespace {

nlohmann::json buildContextJson(const ContextData &context)
{
    nlohmann::json ctx = nlohmann::json::object();

    if (context.systemPrompt) {
        ctx["system_prompt"] = context.systemPrompt->toStdString();
    }

    if (context.prefix) {
        ctx["prefix"] = context.prefix->toStdString();
    }
    if (context.suffix) {
        ctx["suffix"] = context.suffix->toStdString();
    }

    if (context.filesMetadata && !context.filesMetadata->isEmpty()) {
        nlohmann::json files = nlohmann::json::array();
        for (const auto &file : context.filesMetadata.value()) {
            nlohmann::json fj = nlohmann::json::object();
            fj["file_path"] = file.filePath.toStdString();
            fj["content"] = file.content.toStdString();
            files.push_back(std::move(fj));
        }
        ctx["files_metadata"] = std::move(files);
    }

    // tool_result blocks only carry the tool_use_id; resolve the originating
    // tool name so templates (e.g. Google's functionResponse.name) can emit it.
    QHash<QString, QString> toolNameById;
    if (context.history) {
        for (const auto &msg : context.history.value())
            for (const auto &b : msg.blocks)
                if (b.kind == ContentBlockEntry::Kind::ToolUse)
                    toolNameById.insert(b.toolUseId, b.toolName);
    }

    nlohmann::json history = nlohmann::json::array();
    if (context.history) {
        for (const auto &msg : context.history.value()) {
            nlohmann::json mj = nlohmann::json::object();
            mj["role"] = msg.role.toStdString();

            nlohmann::json blocks = nlohmann::json::array();
            QString flatContent;
            nlohmann::json flatImages = nlohmann::json::array();

            for (const auto &b : msg.blocks) {
                nlohmann::json bj = nlohmann::json::object();
                switch (b.kind) {
                case ContentBlockEntry::Kind::Text:
                    bj["type"] = "text";
                    bj["text"] = b.text.toStdString();
                    flatContent += b.text;
                    break;
                case ContentBlockEntry::Kind::Thinking:
                    bj["type"] = "thinking";
                    bj["thinking"] = b.thinking.toStdString();
                    bj["signature"] = b.signature.toStdString();
                    break;
                case ContentBlockEntry::Kind::RedactedThinking:
                    bj["type"] = "redacted_thinking";
                    bj["data"] = b.signature.toStdString();
                    break;
                case ContentBlockEntry::Kind::ToolUse: {
                    bj["type"] = "tool_use";
                    bj["id"] = b.toolUseId.toStdString();
                    bj["name"] = b.toolName.toStdString();
                    const std::string inputStr
                        = QJsonDocument(b.toolInput).toJson(QJsonDocument::Compact).toStdString();
                    nlohmann::json parsedInput
                        = nlohmann::json::parse(inputStr, nullptr, /*allow_exceptions=*/false);
                    if (parsedInput.is_discarded()) {
                        if (!b.toolInput.isEmpty()) {
                            qWarning("[QodeAssist] tool_use '%s' has unparseable input "
                                     "(serialized as null): %s",
                                     qUtf8Printable(b.toolName),
                                     inputStr.c_str());
                        }
                        parsedInput = nullptr;
                    }
                    bj["input"] = std::move(parsedInput);
                    break;
                }
                case ContentBlockEntry::Kind::ToolResult:
                    bj["type"] = "tool_result";
                    bj["tool_use_id"] = b.toolUseId.toStdString();
                    bj["content"] = b.result.toStdString();
                    bj["name"] = toolNameById.value(b.toolUseId).toStdString();
                    break;
                case ContentBlockEntry::Kind::Image:
                    bj["type"] = "image";
                    bj["data"] = b.imageData.toStdString();
                    bj["media_type"] = b.mediaType.toStdString();
                    bj["is_url"] = b.isImageUrl;
                    {
                        nlohmann::json ij = nlohmann::json::object();
                        ij["data"] = b.imageData.toStdString();
                        ij["media_type"] = b.mediaType.toStdString();
                        ij["is_url"] = b.isImageUrl;
                        flatImages.push_back(std::move(ij));
                    }
                    break;
                }
                blocks.push_back(std::move(bj));
            }

            mj["content"] = flatContent.toStdString();
            if (!flatImages.empty())
                mj["images"] = std::move(flatImages);
            mj["content_blocks"] = std::move(blocks);

            history.push_back(std::move(mj));
        }
    }
    ctx["history"] = std::move(history);

    nlohmann::json data = nlohmann::json::object();
    data["ctx"] = std::move(ctx);
    return data;
}

void registerStandardCallbacks(inja::Environment &env)
{
    // Sandbox: disable filesystem reads from `{% include %}` and reject
    // any include callback. User-authored templates run with full
    // process privileges, so they must not slurp arbitrary files via
    // include directives. File reads happen only through
    // ContextManager-provided callbacks (e.g. read_file()).
    env.set_search_included_templates_in_files(false);
    env.set_include_callback(
        [](const std::filesystem::path &, const std::string &name) -> inja::Template {
            throw inja::FileError(
                "include is disabled in QodeAssist templates: '" + name + "'");
        });

    // Disable inja's `##` line-statement shorthand — collides with
    // Markdown headings inside template bodies. Same rationale as in
    // ContextRenderer; retarget to an unreachable sentinel.
    env.set_line_statement("@@@inja@@@");

    env.add_callback("tojson", 1, [](inja::Arguments &args) -> nlohmann::json {
        return args.at(0)->dump();
    });

    env.add_callback("strip_signature_suffix", 1, [](inja::Arguments &args) -> nlohmann::json {
        std::string content = args.at(0)->get<std::string>();
        const std::string marker = "\n[Signature: ";
        const auto pos = content.find(marker);
        if (pos != std::string::npos) {
            content = content.substr(0, pos);
        }
        return content;
    });

    env.add_callback("filter_skip_role", 2, [](inja::Arguments &args) -> nlohmann::json {
        const nlohmann::json &history = *args.at(0);
        const std::string role = args.at(1)->get<std::string>();
        nlohmann::json result = nlohmann::json::array();
        for (const auto &msg : history) {
            if (msg.contains("role") && msg["role"].get<std::string>() == role) {
                continue;
            }
            result.push_back(msg);
        }
        return result;
    });

    env.add_callback("filter_skip_empty_thinking", 1, [](inja::Arguments &args) -> nlohmann::json {
        const nlohmann::json &history = *args.at(0);
        nlohmann::json result = nlohmann::json::array();
        for (const auto &msg : history) {
            const bool isThinking = msg.value("is_thinking", false);
            const std::string sig = msg.value("signature", "");
            if (isThinking && sig.empty()) {
                continue;
            }
            result.push_back(msg);
        }
        return result;
    });

    env.add_callback(
        "filter_skip_empty_parts_thinking", 1, [](inja::Arguments &args) -> nlohmann::json {
            const nlohmann::json &history = *args.at(0);
            nlohmann::json result = nlohmann::json::array();
            for (const auto &msg : history) {
                const bool isThinking = msg.value("is_thinking", false);
                const std::string content = msg.value("content", "");
                const std::string sig = msg.value("signature", "");
                if (isThinking && content.empty() && sig.empty()) {
                    continue;
                }
                result.push_back(msg);
            }
            return result;
        });
}

} // namespace

std::unique_ptr<JsonPromptTemplate> JsonPromptTemplate::fromConfig(
    const AgentConfig &cfg, QString *error)
{
    auto setError = [&error](const QString &msg) {
        if (error) *error = msg;
    };

    if (cfg.messageFormat.isEmpty()) {
        setError(QStringLiteral("Agent '%1' has empty message_format").arg(cfg.name));
        return nullptr;
    }

    auto tpl = std::unique_ptr<JsonPromptTemplate>(new JsonPromptTemplate);
    tpl->m_name = cfg.name;
    tpl->m_description = cfg.description;
    tpl->m_sampling = cfg.sampling;
    tpl->m_thinking = cfg.thinking;

    registerStandardCallbacks(tpl->m_env);
    try {
        tpl->m_template = tpl->m_env.parse(cfg.messageFormat.toStdString());
    } catch (const std::exception &e) {
        setError(QStringLiteral("Failed to parse jinja for '%1': %2")
                     .arg(cfg.name, QString::fromUtf8(e.what())));
        return nullptr;
    }
    return tpl;
}

std::optional<QJsonObject> JsonPromptTemplate::renderBody(const ContextData &context) const
{
    const nlohmann::json data = buildContextJson(context);

    std::string rendered;
    try {
        std::lock_guard<std::mutex> lock(m_renderMutex);
        rendered = m_env.render(m_template, data);
    } catch (const std::exception &e) {
        qWarning("[QodeAssist] Template '%s' render failed: %s",
                 qUtf8Printable(m_name),
                 e.what());
        return std::nullopt;
    }

    QJsonParseError err;
    const QJsonDocument doc
        = QJsonDocument::fromJson(QByteArray::fromStdString(rendered), &err);
    constexpr std::size_t kMaxRenderedLogChars = 500;
    const std::string truncated = rendered.size() > kMaxRenderedLogChars
        ? rendered.substr(0, kMaxRenderedLogChars) + "... [truncated]"
        : rendered;
    if (err.error != QJsonParseError::NoError) {
        qWarning("[QodeAssist] Template '%s' produced invalid JSON at offset %d: %s\n"
                 "--- raw output (truncated) ---\n%s",
                 qUtf8Printable(m_name),
                 err.offset,
                 qUtf8Printable(err.errorString()),
                 truncated.c_str());
        return std::nullopt;
    }
    if (!doc.isObject()) {
        qWarning("[QodeAssist] Template '%s' rendered a non-object JSON value (truncated):\n%s",
                 qUtf8Printable(m_name),
                 truncated.c_str());
        return std::nullopt;
    }
    return doc.object();
}

namespace {

bool mergeRenderedBody(QJsonObject &request, const std::optional<QJsonObject> &body)
{
    if (!body)
        return false;
    for (auto it = body->constBegin(); it != body->constEnd(); ++it) {
        request.insert(it.key(), it.value());
    }
    return true;
}

void deepMergeInto(QJsonObject &base, const QJsonObject &overlay)
{
    for (auto it = overlay.constBegin(); it != overlay.constEnd(); ++it) {
        const QJsonValue baseVal = base.value(it.key());
        const QJsonValue overlayVal = it.value();
        if (baseVal.isObject() && overlayVal.isObject()) {
            QJsonObject merged = baseVal.toObject();
            deepMergeInto(merged, overlayVal.toObject());
            base[it.key()] = merged;
        } else {
            base[it.key()] = overlayVal;
        }
    }
}

} // namespace

void JsonPromptTemplate::prepareRequest(QJsonObject &request, const ContextData &context) const
{
    mergeRenderedBody(request, renderBody(context));
}

bool JsonPromptTemplate::buildFullRequest(
    QJsonObject &request,
    const ContextData &context,
    bool thinkingEnabled) const
{
    if (!mergeRenderedBody(request, renderBody(context)))
        return false;
    applySampling(request, thinkingEnabled);
    return true;
}

void JsonPromptTemplate::applySampling(QJsonObject &request, bool thinkingEnabled) const
{
    // Merge order: sampling provides defaults → body wins for its own
    // keys → thinking overrides win on top.
    QJsonObject merged = m_sampling;
    deepMergeInto(merged, request);

    if (thinkingEnabled && !m_thinking.isEmpty()) {
        deepMergeInto(merged, m_thinking.value("overrides").toObject());
        deepMergeInto(merged, m_thinking.value("request_block").toObject());
    }

    request = std::move(merged);
}

} // namespace QodeAssist::Templates
