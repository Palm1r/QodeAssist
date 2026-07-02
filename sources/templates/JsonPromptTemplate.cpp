// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "JsonPromptTemplate.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
                    nlohmann::json parsedInput = nlohmann::json::parse(inputStr, nullptr, false);
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

std::string stripTrailingCommas(const std::string &in)
{
    std::string out;
    out.reserve(in.size());
    bool inString = false;
    bool escaped = false;
    for (std::size_t i = 0; i < in.size(); ++i) {
        const char c = in[i];
        if (inString) {
            out.push_back(c);
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                inString = false;
            continue;
        }
        if (c == '"') {
            inString = true;
            out.push_back(c);
            continue;
        }
        if (c == ',') {
            std::size_t j = i + 1;
            while (j < in.size()
                   && (in[j] == ' ' || in[j] == '\t' || in[j] == '\n' || in[j] == '\r'))
                ++j;
            if (j < in.size() && (in[j] == '}' || in[j] == ']'))
                continue;
        }
        out.push_back(c);
    }
    return out;
}

void setIncludeResolver(inja::Environment &env, std::vector<QString> roots)
{
    inja::Environment *envPtr = &env;
    env.set_include_callback(
        [envPtr, roots = std::move(roots)](const std::filesystem::path &, const std::string &name)
            -> inja::Template {
            const QString rel = QString::fromStdString(name);
            if (rel.contains(QStringLiteral("..")) || rel.startsWith(QLatin1Char('/'))) {
                throw inja::FileError("include rejected (path traversal): '" + name + "'");
            }
            for (const QString &root : roots) {
                QFile f(root + QLatin1Char('/') + rel);
                if (f.open(QIODevice::ReadOnly | QIODevice::Text))
                    return envPtr->parse(QString::fromUtf8(f.readAll()).toStdString());
            }
            throw inja::FileError("include not found in partials roots: '" + name + "'");
        });
}

void registerStandardCallbacks(inja::Environment &env)
{
    env.set_search_included_templates_in_files(false);

    env.set_line_statement("@@@inja@@@");

    env.add_callback("tojson", 1, [](inja::Arguments &args) -> nlohmann::json {
        return args.at(0)->dump();
    });

    env.add_callback("filter_by_type", 2, [](inja::Arguments &args) -> nlohmann::json {
        const nlohmann::json &blocks = *args.at(0);
        const std::string type = args.at(1)->get<std::string>();
        nlohmann::json result = nlohmann::json::array();
        if (blocks.is_array()) {
            for (const auto &b : blocks) {
                if (b.is_object() && b.value("type", std::string{}) == type)
                    result.push_back(b);
            }
        }
        return result;
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
}

ContextData makeValidationContext()
{
    const QString hostile = QStringLiteral("va\"li\\da\ntion");

    ContextData ctx;
    ctx.systemPrompt = QStringLiteral("system ") + hostile;
    ctx.prefix = QStringLiteral("prefix ") + hostile;
    ctx.suffix = QStringLiteral("suffix ") + hostile;

    QVector<Message> history;
    history.append(Message::text(QStringLiteral("user"), QStringLiteral("hello ") + hostile));

    Message asst;
    asst.role = QStringLiteral("assistant");
    {
        ContentBlockEntry th;
        th.kind = ContentBlockEntry::Kind::Thinking;
        th.thinking = QStringLiteral("reasoning ") + hostile;
        th.signature = QStringLiteral("sig");
        asst.blocks.append(th);
        ContentBlockEntry rth;
        rth.kind = ContentBlockEntry::Kind::RedactedThinking;
        rth.signature = QStringLiteral("sig");
        asst.blocks.append(rth);
        ContentBlockEntry t;
        t.kind = ContentBlockEntry::Kind::Text;
        t.text = QStringLiteral("hi ") + hostile;
        asst.blocks.append(t);
        ContentBlockEntry tu;
        tu.kind = ContentBlockEntry::Kind::ToolUse;
        tu.toolUseId = QStringLiteral("call_1");
        tu.toolName = QStringLiteral("read_file");
        tu.toolInput = QJsonObject{{QStringLiteral("path"), hostile}};
        asst.blocks.append(tu);
    }
    history.append(asst);

    Message toolMsg;
    toolMsg.role = QStringLiteral("user");
    {
        ContentBlockEntry tr;
        tr.kind = ContentBlockEntry::Kind::ToolResult;
        tr.toolUseId = QStringLiteral("call_1");
        tr.result = QStringLiteral("ok ") + hostile;
        toolMsg.blocks.append(tr);
    }
    history.append(toolMsg);

    Message imgMsg;
    imgMsg.role = QStringLiteral("user");
    {
        ContentBlockEntry te;
        te.kind = ContentBlockEntry::Kind::Text;
        te.text = QStringLiteral("look ") + hostile;
        imgMsg.blocks.append(te);
        ContentBlockEntry im;
        im.kind = ContentBlockEntry::Kind::Image;
        im.imageData = QStringLiteral("AAAA");
        im.mediaType = QStringLiteral("image/png");
        imgMsg.blocks.append(im);
    }
    history.append(imgMsg);

    ctx.history = history;
    return ctx;
}

} // namespace

std::unique_ptr<JsonPromptTemplate> JsonPromptTemplate::fromConfig(
    const AgentConfig &cfg, QString *error)
{
    auto setError = [&error](const QString &msg) {
        if (error) *error = msg;
    };

    if (cfg.body.isEmpty()) {
        setError(QStringLiteral("Agent '%1' has empty [body]").arg(cfg.name));
        return nullptr;
    }

    auto tpl = std::unique_ptr<JsonPromptTemplate>(new JsonPromptTemplate);
    tpl->m_name = cfg.name;
    tpl->m_description = cfg.description;
    tpl->m_body = cfg.body;

    tpl->m_partialRoots.push_back(QStringLiteral(":/agents"));
    if (cfg.isUserSource()) {
        const QString dir = QFileInfo(cfg.sourcePath).absolutePath();
        if (!dir.isEmpty())
            tpl->m_partialRoots.push_back(dir);
    }

    registerStandardCallbacks(tpl->m_env);
    setIncludeResolver(tpl->m_env, tpl->m_partialRoots);

    if (!tpl->renderBody(makeValidationContext())) {
        setError(QStringLiteral(
                     "Agent '%1' [body] failed to render to valid JSON "
                     "(see log)")
                     .arg(cfg.name));
        return nullptr;
    }
    return tpl;
}

namespace {

bool renderValue(
    inja::Environment &env,
    const QString &tplName,
    const QJsonValue &in,
    const nlohmann::json &data,
    QJsonValue &out,
    bool &omit)
{
    omit = false;

    if (in.isObject()) {
        QJsonObject obj;
        const QJsonObject src = in.toObject();
        for (auto it = src.constBegin(); it != src.constEnd(); ++it) {
            QJsonValue v;
            bool om = false;
            if (!renderValue(env, tplName, it.value(), data, v, om))
                return false;
            if (!om)
                obj.insert(it.key(), v);
        }
        out = obj;
        return true;
    }

    if (in.isArray()) {
        QJsonArray arr;
        const QJsonArray src = in.toArray();
        for (const QJsonValue &elem : src) {
            QJsonValue v;
            bool om = false;
            if (!renderValue(env, tplName, elem, data, v, om))
                return false;
            if (!om)
                arr.append(v);
        }
        out = arr;
        return true;
    }

    if (!in.isString()) {
        out = in;
        return true;
    }

    const QString s = in.toString();
    if (!s.contains(QStringLiteral("{{")) && !s.contains(QStringLiteral("{%"))) {
        out = in;
        return true;
    }

    std::string rendered;
    try {
        rendered = env.render(s.toStdString(), data);
    } catch (const std::exception &e) {
        qWarning(
            "[QodeAssist] Template '%s' field render failed: %s", qUtf8Printable(tplName), e.what());
        return false;
    }

    rendered = stripTrailingCommas(rendered);
    if (QString::fromStdString(rendered).trimmed().isEmpty()) {
        omit = true;
        return true;
    }

    const std::string wrapped = "{\"v\":" + rendered + "}";
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(wrapped), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        const QString snippet = QString::fromStdString(rendered).left(500);
        qWarning(
            "[QodeAssist] Template '%s' field produced invalid JSON: %s\n"
            "--- rendered (truncated) ---\n%s",
            qUtf8Printable(tplName),
            qUtf8Printable(perr.errorString()),
            qUtf8Printable(snippet));
        return false;
    }
    out = doc.object().value(QStringLiteral("v"));
    return true;
}

bool mergeRenderedBody(QJsonObject &request, const std::optional<QJsonObject> &body)
{
    if (!body)
        return false;
    for (auto it = body->constBegin(); it != body->constEnd(); ++it)
        request.insert(it.key(), it.value());
    return true;
}

} // namespace

std::optional<QJsonObject> JsonPromptTemplate::renderBody(const ContextData &context) const
{
    const nlohmann::json data = buildContextJson(context);

    std::lock_guard<std::mutex> lock(m_renderMutex);
    QJsonObject request;
    for (auto it = m_body.constBegin(); it != m_body.constEnd(); ++it) {
        QJsonValue v;
        bool omit = false;
        if (!renderValue(m_env, m_name, it.value(), data, v, omit))
            return std::nullopt;
        if (!omit)
            request.insert(it.key(), v);
    }
    return request;
}

bool JsonPromptTemplate::buildFullRequest(QJsonObject &request, const ContextData &context) const
{
    return mergeRenderedBody(request, renderBody(context));
}

} // namespace QodeAssist::Templates
