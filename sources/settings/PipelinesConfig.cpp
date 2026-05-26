// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PipelinesConfig.hpp"

#include <coreplugin/icore.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <toml++/toml.hpp>

#include <exception>

#include "Logger.hpp"
#include "TomlWriter.hpp"
#include <AgentFactory.hpp>

namespace QodeAssist::Settings {

namespace {

constexpr const char *kSection = "pipelines";
constexpr const char *kCodeCompletion = "code_completion";
constexpr const char *kChatAssistant = "chat_assistant";
constexpr const char *kChatCompression = "chat_compression";
constexpr const char *kQuickRefactor = "quick_refactor";
constexpr int kMaxAgentNameLen = 256;

QString trimAndCap(const QString &raw)
{
    QString s = raw.trimmed();
    if (s.size() > kMaxAgentNameLen)
        s.truncate(kMaxAgentNameLen);
    return s;
}

QStringList toStringList(const toml::node *node, const QString &slotKey, bool *schemaOk)
{
    QStringList out;
    if (!node)
        return out;
    const auto *arr = node->as_array();
    if (!arr) {
        LOG_MESSAGE(QStringLiteral(
            "[Pipelines] schema error: '%1' must be an array of strings, got non-array")
                        .arg(slotKey));
        if (schemaOk)
            *schemaOk = false;
        return out;
    }
    out.reserve(static_cast<qsizetype>(arr->size()));
    for (size_t i = 0; i < arr->size(); ++i) {
        const auto &el = (*arr)[i];
        if (const auto *s = el.as_string()) {
            const QString name = trimAndCap(QString::fromStdString(s->get()));
            if (name.isEmpty())
                continue;
            if (out.contains(name)) {
                LOG_MESSAGE(QStringLiteral("[Pipelines] '%1' element #%2 is a duplicate, dropping")
                                .arg(slotKey)
                                .arg(i));
                continue;
            }
            out.append(name);
        } else {
            LOG_MESSAGE(QStringLiteral("[Pipelines] '%1' element #%2 is not a string, dropping")
                            .arg(slotKey)
                            .arg(i));
            if (schemaOk)
                *schemaOk = false;
        }
    }
    return out;
}

void fillMissingFromDefaults(PipelineRosters &r, const toml::table &section)
{
    const PipelineRosters defs = PipelineRosters::defaults();
    if (!section.contains(kCodeCompletion))
        r.codeCompletion = defs.codeCompletion;
    if (!section.contains(kChatAssistant))
        r.chatAssistant = defs.chatAssistant;
    if (!section.contains(kChatCompression))
        r.chatCompression = defs.chatCompression;
    if (!section.contains(kQuickRefactor))
        r.quickRefactor = defs.quickRefactor;
}

} // namespace

PipelineRosters PipelineRosters::defaults()
{
    PipelineRosters r;
    r.codeCompletion = {QStringLiteral("Ollama Qwen2.5-Coder Completion")};
    r.chatAssistant = {QStringLiteral("Ollama Chat")};
    r.chatCompression = {QStringLiteral("Ollama Compression")};
    r.quickRefactor = {QStringLiteral("Ollama Quick Refactor")};
    return r;
}

QString PipelinesConfig::filePath()
{
    return Core::ICore::userResourcePath(QStringLiteral("qodeassist/config/pipelines.toml"))
        .toFSPathString();
}

PipelinesLoadResult PipelinesConfig::load()
{
    PipelinesLoadResult result;
    const QString path = filePath();
    QFile f(path);
    if (!f.exists()) {
        result.rosters = PipelineRosters::defaults();
        result.status = PipelinesLoadStatus::FileMissing;
        return result;
    }
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.rosters = PipelineRosters::defaults();
        result.status = PipelinesLoadStatus::ParseError;
        result.message = QStringLiteral("cannot open %1: %2").arg(path, f.errorString());
        LOG_MESSAGE(QStringLiteral("[Pipelines] %1").arg(result.message));
        return result;
    }
    const QByteArray contents = f.readAll();
    f.close();

    toml::table tbl;
    try {
        tbl = toml::parse(std::string_view(contents.constData(),
                                            static_cast<size_t>(contents.size())),
                          path.toStdString());
    } catch (const toml::parse_error &e) {
        result.rosters = PipelineRosters::defaults();
        result.status = PipelinesLoadStatus::ParseError;
        result.message = QStringLiteral("parse error in %1: %2")
                             .arg(path, QString::fromStdString(std::string(e.description())));
        LOG_MESSAGE(QStringLiteral("[Pipelines] %1").arg(result.message));
        return result;
    } catch (const std::exception &e) {
        result.rosters = PipelineRosters::defaults();
        result.status = PipelinesLoadStatus::ParseError;
        result.message = QStringLiteral("error reading %1: %2")
                             .arg(path, QString::fromUtf8(e.what()));
        LOG_MESSAGE(QStringLiteral("[Pipelines] %1").arg(result.message));
        return result;
    } catch (...) {
        result.rosters = PipelineRosters::defaults();
        result.status = PipelinesLoadStatus::ParseError;
        result.message = QStringLiteral("unknown error reading %1").arg(path);
        LOG_MESSAGE(QStringLiteral("[Pipelines] %1").arg(result.message));
        return result;
    }

    const auto *section = tbl[kSection].as_table();
    if (!section) {
        LOG_MESSAGE(QStringLiteral("[Pipelines] no [pipelines] section in %1; using defaults")
                        .arg(path));
        result.rosters = PipelineRosters::defaults();
        result.status = PipelinesLoadStatus::SchemaError;
        result.message = QStringLiteral("missing [pipelines] section");
        return result;
    }

    bool schemaOk = true;
    result.rosters.codeCompletion
        = toStringList((*section)[kCodeCompletion].node(), kCodeCompletion, &schemaOk);
    result.rosters.chatAssistant
        = toStringList((*section)[kChatAssistant].node(), kChatAssistant, &schemaOk);
    result.rosters.chatCompression
        = toStringList((*section)[kChatCompression].node(), kChatCompression, &schemaOk);
    result.rosters.quickRefactor
        = toStringList((*section)[kQuickRefactor].node(), kQuickRefactor, &schemaOk);

    fillMissingFromDefaults(result.rosters, *section);

    if (!schemaOk) {
        result.status = PipelinesLoadStatus::SchemaError;
        result.message = QStringLiteral("some entries had wrong type; see log");
    }
    return result;
}

bool PipelinesConfig::save(const PipelineRosters &rosters, QString *errorOut)
{
    const QString path = filePath();
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        const QString err = QStringLiteral("cannot create configuration directory: %1")
                                .arg(info.absolutePath());
        LOG_MESSAGE(QStringLiteral("[Pipelines] %1").arg(err));
        if (errorOut)
            *errorOut = err;
        return false;
    }

    TomlSerializer::TomlWriter w;
    w.writeComment(QStringLiteral(
        "QodeAssist pipelines — slot → ordered list of agent names."));
    w.writeComment(QStringLiteral(
        "The router walks each list top-down at request time and uses"));
    w.writeComment(QStringLiteral("the first matching agent."));
    w.writeBlankLine();
    w.writeTableHeader(QString::fromUtf8(kSection));
    w.writeStringArray(QString::fromUtf8(kCodeCompletion), rosters.codeCompletion);
    w.writeStringArray(QString::fromUtf8(kChatAssistant), rosters.chatAssistant);
    w.writeStringArray(QString::fromUtf8(kChatCompression), rosters.chatCompression);
    w.writeStringArray(QString::fromUtf8(kQuickRefactor), rosters.quickRefactor);

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut)
            *errorOut = out.errorString();
        return false;
    }
    const QByteArray payload = w.toUtf8();
    const qint64 written = out.write(payload);
    if (written != payload.size()) {
        const QString err = QStringLiteral("short write (%1/%2): %3")
                                .arg(written)
                                .arg(payload.size())
                                .arg(out.errorString());
        LOG_MESSAGE(QStringLiteral("[Pipelines] %1").arg(err));
        out.cancelWriting();
        if (errorOut)
            *errorOut = err;
        return false;
    }
    if (!out.commit()) {
        if (errorOut)
            *errorOut = out.errorString();
        return false;
    }
    return true;
}

bool PipelinesConfig::validate(const QodeAssist::AgentFactory &factory, QString *errorOut)
{
    PipelinesLoadResult lr = load();
    PipelineRosters &r = lr.rosters;
    bool changed = false;

    auto fix = [&](QStringList &current) {
        QStringList kept;
        kept.reserve(current.size());
        for (const QString &raw : current) {
            const QString name = trimAndCap(raw);
            if (name.isEmpty() || kept.contains(name))
                continue;
            if (factory.configByName(name))
                kept.append(name);
        }
        if (kept != current) {
            current = std::move(kept);
            changed = true;
        }
    };

    fix(r.codeCompletion);
    fix(r.chatAssistant);
    fix(r.chatCompression);
    fix(r.quickRefactor);

    if (!changed && lr.status == PipelinesLoadStatus::Ok)
        return true;

    QString saveErr;
    if (!save(r, &saveErr)) {
        const QString msg = QStringLiteral("failed to persist after validation: %1").arg(saveErr);
        LOG_MESSAGE(QStringLiteral("[Pipelines] %1").arg(msg));
        if (errorOut)
            *errorOut = msg;
        return false;
    }
    return true;
}

} // namespace QodeAssist::Settings
