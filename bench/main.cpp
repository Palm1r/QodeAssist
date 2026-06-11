// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>
#include <QTimer>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/BaseTool.hpp>
#include <LLMQore/ContentBlocks.hpp>
#include <LLMQore/ToolRegistry.hpp>
#include <LLMQore/ToolResult.hpp>
#include <LLMQore/ToolsManager.hpp>

#include <Agent.hpp>
#include <AgentConfig.hpp>
#include <AgentFactory.hpp>
#include <ContextData.hpp>
#include <ContextRenderer.hpp>
#include <PluginBlocks.hpp>
#include <GenericProvider.hpp>
#include <Provider.hpp>
#include <ProviderInstance.hpp>
#include <ProviderInstanceFactory.hpp>
#include <ProviderSecretsStore.hpp>
#include <ResponseEvent.hpp>
#include <Session.hpp>

using namespace QodeAssist;

namespace {

QTextStream &out()
{
    static QTextStream s(stdout);
    return s;
}

QTextStream &err()
{
    static QTextStream s(stderr);
    return s;
}

QString readStdin()
{
    QTextStream in(stdin);
    return in.readAll();
}

QHash<QString, QString> parseEnvFile(const QString &path, QString *errorOut)
{
    QHash<QString, QString> map;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = QStringLiteral("cannot open env file: %1").arg(path);
        return map;
    }
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1String("export ")))
            line = line.mid(7).trimmed();
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        QString value = line.mid(eq + 1).trimmed();
        if (value.size() >= 2
            && ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
                || (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\''))))) {
            value = value.mid(1, value.size() - 2);
        }
        map.insert(key, value);
    }
    return map;
}

QStringList apiKeyCandidates(const QString &clientApi, const QString &apiKeyRef)
{
    QStringList c;
    if (!apiKeyRef.isEmpty())
        c << apiKeyRef;
    if (clientApi == QLatin1String("Claude"))
        c << QStringLiteral("ANTHROPIC_API_KEY");
    else if (clientApi.startsWith(QLatin1String("OpenAI")))
        c << QStringLiteral("OPENAI_API_KEY");
    else if (clientApi == QLatin1String("Mistral AI"))
        c << QStringLiteral("MISTRAL_API_KEY");
    else if (clientApi == QLatin1String("Codestral"))
        c << QStringLiteral("CODESTRAL_API_KEY");
    else if (clientApi == QLatin1String("Google AI"))
        c << QStringLiteral("GEMINI_API_KEY") << QStringLiteral("GOOGLE_API_KEY");
    else if (clientApi == QLatin1String("OpenRouter"))
        c << QStringLiteral("OPENROUTER_API_KEY");

    QString derived = clientApi.toUpper();
    derived.replace(QRegularExpression(QStringLiteral("[^A-Z0-9]+")), QStringLiteral("_"));
    derived = derived.trimmed();
    while (derived.startsWith(QLatin1Char('_')))
        derived.remove(0, 1);
    while (derived.endsWith(QLatin1Char('_')))
        derived.chop(1);
    if (!derived.isEmpty())
        c << derived + QStringLiteral("_API_KEY");
    return c;
}

QString resolveApiKey(
    const QHash<QString, QString> &envFile, const QString &clientApi, const QString &apiKeyRef)
{
    for (const QString &name : apiKeyCandidates(clientApi, apiKeyRef)) {
        auto it = envFile.constFind(name);
        if (it != envFile.constEnd() && !it.value().isEmpty())
            return it.value();
        const QByteArray fromProc = qgetenv(name.toUtf8().constData());
        if (!fromProc.isEmpty())
            return QString::fromUtf8(fromProc);
    }
    return {};
}

QString imageMediaType(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("png"))
        return QStringLiteral("image/png");
    if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg"))
        return QStringLiteral("image/jpeg");
    if (ext == QLatin1String("gif"))
        return QStringLiteral("image/gif");
    if (ext == QLatin1String("webp"))
        return QStringLiteral("image/webp");
    return {};
}

class BenchEchoTool : public LLMQore::BaseTool
{
public:
    using BaseTool::BaseTool;
    QString id() const override { return QStringLiteral("bench_echo"); }
    QString displayName() const override { return QStringLiteral("Bench echo"); }
    QString description() const override
    {
        return QStringLiteral("Echoes the given text back verbatim. "
                              "Use whenever the user asks to echo something.");
    }
    QJsonObject parametersSchema() const override
    {
        return QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"),
             QJsonObject{
                 {QStringLiteral("text"),
                  QJsonObject{
                      {QStringLiteral("type"), QStringLiteral("string")},
                      {QStringLiteral("description"), QStringLiteral("Text to echo back")}}}}},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("text")}}};
    }
    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input) override
    {
        return QtFuture::makeReadyValueFuture(LLMQore::ToolResult::text(
            QStringLiteral("echo: %1").arg(input.value(QStringLiteral("text")).toString())));
    }
};

class BenchAddTool : public LLMQore::BaseTool
{
public:
    using BaseTool::BaseTool;
    QString id() const override { return QStringLiteral("bench_add"); }
    QString displayName() const override { return QStringLiteral("Bench add"); }
    QString description() const override
    {
        return QStringLiteral("Adds two numbers and returns the sum. "
                              "Use whenever the user asks to add numbers.");
    }
    QJsonObject parametersSchema() const override
    {
        return QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"),
             QJsonObject{
                 {QStringLiteral("a"),
                  QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}},
                 {QStringLiteral("b"),
                  QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}}}},
            {QStringLiteral("required"),
             QJsonArray{QStringLiteral("a"), QStringLiteral("b")}}};
    }
    QFuture<LLMQore::ToolResult> executeAsync(const QJsonObject &input) override
    {
        const double sum = input.value(QStringLiteral("a")).toDouble()
                           + input.value(QStringLiteral("b")).toDouble();
        return QtFuture::makeReadyValueFuture(
            LLMQore::ToolResult::text(QString::number(sum)));
    }
};

void printEvent(const ResponseEvent &ev, bool showThinking)
{
    switch (ev.kind()) {
    case ResponseEvent::Kind::TextDelta:
        if (const auto *d = ev.as<ResponseEvents::TextDelta>()) {
            out() << d->text;
            out().flush();
        }
        break;
    case ResponseEvent::Kind::ThinkingDelta:
        if (showThinking) {
            if (const auto *d = ev.as<ResponseEvents::ThinkingDelta>()) {
                err() << d->thinking;
                err().flush();
            }
        }
        break;
    case ResponseEvent::Kind::ToolCallStart:
        if (const auto *d = ev.as<ResponseEvents::ToolCallStart>())
            err() << "\n[tool-call] " << d->name << " (" << d->id << ")\n";
        break;
    case ResponseEvent::Kind::ToolCallEnd:
        if (const auto *d = ev.as<ResponseEvents::ToolCallEnd>()) {
            const QString args
                = QString::fromUtf8(QJsonDocument(d->finalArgs).toJson(QJsonDocument::Compact));
            err() << "[tool-args] " << args << "\n";
        }
        break;
    case ResponseEvent::Kind::ToolResult:
        if (const auto *d = ev.as<ResponseEvents::ToolResult>())
            err() << "[tool-result" << (d->isError ? " ERROR" : "") << "] " << d->text << "\n";
        break;
    case ResponseEvent::Kind::Usage:
        if (const auto *d = ev.as<ResponseEvents::Usage>()) {
            err() << "\n[usage] in=" << d->inputTokens << " out=" << d->outputTokens
                  << " cached=" << d->cachedTokens << " reasoning=" << d->reasoningTokens << "\n";
        }
        break;
    case ResponseEvent::Kind::Error:
        if (const auto *d = ev.as<ResponseEvents::Error>())
            err() << "\n[error] " << d->message << "\n";
        break;
    case ResponseEvent::Kind::MessageStart:
    case ResponseEvent::Kind::ToolCallArgsDelta:
    case ResponseEvent::Kind::MessageStop:
        break;
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("QtProject"));
    QCoreApplication::setApplicationName(QStringLiteral("QtCreator"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "QodeAssist bench — drive an agent through the live session pipeline.");
    parser.addHelpOption();

    QCommandLineOption listOpt(QStringList{"l", "list"}, "List available agent profiles and exit.");
    QCommandLineOption agentOpt(
        QStringList{"a", "agent"}, "Agent profile name to run.", "name");
    QCommandLineOption fileOpt(
        QStringList{"f", "file"}, "Load an agent from a TOML file instead of by name.", "path");
    QCommandLineOption promptOpt(
        QStringList{"p", "prompt"},
        "Prompt text. Repeatable: each occurrence is one chat turn, sent after the "
        "previous turn finishes (history is replayed through the agent template). "
        "If omitted, positional args or stdin are used as a single turn.",
        "text");
    QCommandLineOption noThinkingOpt("no-thinking", "Hide thinking deltas from output.");
    QCommandLineOption envOpt(
        QStringList{"e", "env"},
        "Read API keys from a dotenv file (KEY=VALUE per line). Defaults to ./.env if present.",
        "path");
    QCommandLineOption apiKeyOpt(
        "api-key", "API key to use for the agent's provider (overrides env/settings).", "value");
    QCommandLineOption timeoutOpt(
        "timeout",
        "Network transfer timeout in seconds (a stalled stream fails instead of hanging). "
        "Default 60, 0 disables.",
        "seconds");
    QCommandLineOption projectDirOpt(
        QStringList{"C", "project-dir"},
        "Project root for the agent's context (${PROJECT_DIR}). Defaults to the current directory.",
        "path");
    QCommandLineOption imageOpt(
        QStringList{"i", "image"},
        "Attach an image file (png/jpeg/gif/webp). Repeatable. Requires a vision-capable agent.",
        "path");
    QCommandLineOption mcpOpt(
        "mcp",
        "Load MCP servers from a JSON config (mcpServers map) to give the agent executable tools.",
        "path");
    QCommandLineOption builtinToolsOpt(
        "builtin-tools",
        "Register local test tools (bench_echo, bench_add) and force tools on. "
        "Lets the model exercise tool calls without an MCP server, e.g. "
        "-p \"echo hello via the tool\" -p \"now add 2 and 3\".");
    QCommandLineOption fimOpt(
        "fim",
        "Fill-in-the-middle completion mode: send prompt as the prefix and --suffix as the suffix.");
    QCommandLineOption suffixOpt(
        "suffix", "Suffix code after the cursor (FIM mode only).", "text");
    parser.addOption(listOpt);
    parser.addOption(agentOpt);
    parser.addOption(fileOpt);
    parser.addOption(promptOpt);
    parser.addOption(noThinkingOpt);
    parser.addOption(envOpt);
    parser.addOption(apiKeyOpt);
    parser.addOption(timeoutOpt);
    parser.addOption(projectDirOpt);
    parser.addOption(imageOpt);
    parser.addOption(mcpOpt);
    parser.addOption(builtinToolsOpt);
    parser.addOption(fimOpt);
    parser.addOption(suffixOpt);
    parser.addPositionalArgument("prompt", "Prompt text (alternative to --prompt).", "[prompt...]");
    parser.process(app);

    Providers::registerBuiltinProviders();

    auto *instances = new Providers::ProviderInstanceFactory(&app);
    auto *secrets = new Providers::ProviderSecretsStore(&app);
    auto *agentFactory = new AgentFactory(instances, secrets, &app);

    if (parser.isSet(listOpt)) {
        const QStringList names = agentFactory->configNames();
        if (names.isEmpty())
            err() << "No agent profiles found.\n";
        for (const QString &n : names)
            out() << n << "\n";
        return 0;
    }

    QString error;
    Agent *agent = nullptr;
    if (parser.isSet(fileOpt)) {
        agent = agentFactory->createFromFile(parser.value(fileOpt), &app, &error);
    } else if (parser.isSet(agentOpt)) {
        agent = agentFactory->create(parser.value(agentOpt), &app, &error);
    } else {
        err() << "Specify an agent with --agent <name> or --file <path>, or use --list.\n";
        return 2;
    }

    if (!agent) {
        err() << "Failed to create agent: " << error << "\n";
        return 1;
    }

    const bool fimMode = parser.isSet(fimOpt);

    Session *session = new Session(agent, &app);
    if (!session->isValid()) {
        err() << "Failed to create session: " << session->invalidReason() << "\n";
        return 1;
    }

    {
        bool ok = false;
        const int timeoutSecs = parser.isSet(timeoutOpt)
                                    ? parser.value(timeoutOpt).toInt(&ok)
                                    : 60;
        if (parser.isSet(timeoutOpt) && !ok) {
            err() << "Invalid --timeout value.\n";
            return 2;
        }
        if (timeoutSecs > 0)
            if (auto *client = session->client())
                client->setTransferTimeout(timeoutSecs * 1000);
    }

    {
        QHash<QString, QString> envFile;
        QString envPath = parser.value(envOpt);
        if (envPath.isEmpty() && QFile::exists(QStringLiteral(".env")))
            envPath = QStringLiteral(".env");
        if (!envPath.isEmpty()) {
            QString envErr;
            envFile = parseEnvFile(envPath, &envErr);
            if (!envErr.isEmpty())
                err() << "[env] " << envErr << "\n";
        }

        QString key = parser.value(apiKeyOpt);
        if (key.isEmpty()) {
            const AgentConfig &cfg = agent->config();
            const Providers::ProviderInstance *inst
                = instances->instanceByName(cfg.providerInstance);
            if (inst)
                key = resolveApiKey(envFile, inst->clientApi, inst->apiKeyRef);
        }
        if (!key.isEmpty() && agent->provider())
            agent->provider()->setApiKey(key);
    }

    {
        Templates::ContextRenderer::Bindings bindings;
        bindings.projectDir = parser.isSet(projectDirOpt)
                                  ? QDir(parser.value(projectDirOpt)).absolutePath()
                                  : QDir::currentPath();
        bindings.homeDir = QDir::homePath();
        session->setContextBindings(bindings);
    }

    const QStringList imagePaths = parser.values(imageOpt);

    QStringList turns = parser.values(promptOpt);
    if (turns.isEmpty()) {
        QString prompt = parser.positionalArguments().join(QLatin1Char(' '));
        if (prompt.isEmpty() && imagePaths.isEmpty())
            prompt = readStdin().trimmed();
        if (!prompt.isEmpty())
            turns << prompt;
    }
    if (turns.isEmpty() && imagePaths.isEmpty()) {
        err() << "Empty prompt.\n";
        return 2;
    }
    if (fimMode && turns.size() > 1) {
        err() << "FIM mode takes a single prompt; extra turns ignored.\n";
        turns = {turns.first()};
    }

    if (!imagePaths.isEmpty() && !session->supportsImages())
        err() << "[warning] agent's provider does not advertise image support.\n";

    std::optional<bool> toolsOverride;
    if (parser.isSet(builtinToolsOpt) || parser.isSet(mcpOpt))
        toolsOverride = true;

    if (parser.isSet(builtinToolsOpt)) {
        auto *tools = session->client()->tools();
        tools->addTool(new BenchEchoTool(tools));
        tools->addTool(new BenchAddTool(tools));
        err() << "[tools] registered bench_echo, bench_add\n";
    }

    const bool showThinking = !parser.isSet(noThinkingOpt);
    int exitCode = 0;
    int nextTurn = 0;
    std::function<void()> sendNextTurn;

    QObject::connect(
        session, &Session::event, &app, [showThinking](const ResponseEvent &ev) {
            printEvent(ev, showThinking);
        });
    QObject::connect(
        session, &Session::finished, &app,
        [&](const LLMQore::RequestID &, const QString &reason) {
            err() << "\n[done] stopReason=" << (reason.isEmpty() ? "<none>" : reason) << "\n";
            if (!fimMode && nextTurn < turns.size()) {
                sendNextTurn();
                return;
            }
            QCoreApplication::quit();
        });
    QObject::connect(
        session, &Session::failed, &app,
        [&](const LLMQore::RequestID &, const QodeAssist::ErrorInfo &info) {
            err() << "\n[failed] " << info.message << "\n";
            exitCode = 1;
            QCoreApplication::quit();
        });
    QObject::connect(
        session, &Session::cancelled, &app, [&](const LLMQore::RequestID &) {
            err() << "\n[cancelled]\n";
            QCoreApplication::quit();
        });

    sendNextTurn = [&] {
        std::vector<std::unique_ptr<LLMQore::ContentBlock>> blocks;
        if (nextTurn == 0) {
            for (const QString &imgPath : imagePaths) {
                QFile img(imgPath);
                if (!img.open(QIODevice::ReadOnly)) {
                    err() << "[image] cannot open: " << imgPath << "\n";
                    exitCode = 1;
                    QCoreApplication::quit();
                    return;
                }
                const QString media = imageMediaType(imgPath);
                if (media.isEmpty()) {
                    err() << "[image] unsupported type: " << imgPath << "\n";
                    exitCode = 1;
                    QCoreApplication::quit();
                    return;
                }
                const QString b64 = QString::fromLatin1(img.readAll().toBase64());
                blocks.push_back(std::make_unique<LLMQore::ImageContent>(
                    b64, media, LLMQore::ImageContent::ImageSourceType::Base64));
            }
        }
        const QString text = turns.value(nextTurn);
        if (!text.isEmpty())
            blocks.push_back(std::make_unique<LLMQore::TextContent>(text));
        if (blocks.empty()) {
            err() << "Nothing to send.\n";
            exitCode = 1;
            QCoreApplication::quit();
            return;
        }
        if (turns.size() > 1)
            err() << "\n[turn " << (nextTurn + 1) << "/" << turns.size() << "] " << text << "\n";
        ++nextTurn;
        if (session->send(std::move(blocks), toolsOverride).isEmpty()) {
            err() << "Failed to dispatch request: " << session->lastError().message << "\n";
            exitCode = 1;
            QCoreApplication::quit();
        }
    };

    auto dispatch = [&] {
        if (fimMode) {
            const QString prefix = turns.value(0);
            const QString suffix = parser.isSet(suffixOpt) ? parser.value(suffixOpt) : QString();
            std::vector<std::unique_ptr<LLMQore::ContentBlock>> blocks;
            blocks.push_back(std::make_unique<QodeAssist::CompletionContent>(prefix, suffix));
            if (session->send(std::move(blocks), /*toolsOverride=*/false).isEmpty()) {
                err() << "Failed to dispatch FIM request: " << session->lastError().message << "\n";
                exitCode = 1;
                QCoreApplication::quit();
            }
            return;
        }
        sendNextTurn();
    };

    if (parser.isSet(mcpOpt)) {
        const QString mcpPath = parser.value(mcpOpt);
        QFile mcpFile(mcpPath);
        if (!mcpFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            err() << "[mcp] cannot open config: " << mcpPath << "\n";
            return 2;
        }
        QJsonParseError jerr;
        const QJsonDocument mcpDoc = QJsonDocument::fromJson(mcpFile.readAll(), &jerr);
        if (jerr.error != QJsonParseError::NoError || !mcpDoc.isObject()) {
            err() << "[mcp] invalid JSON config: " << jerr.errorString() << "\n";
            return 2;
        }
        auto *client = session->client();
        if (!client) {
            err() << "[mcp] session has no client.\n";
            return 1;
        }
        auto *tools = client->tools();
        tools->loadMcpServers(mcpDoc.object());
        err() << "[mcp] loading servers, waiting for tools...\n";

        auto dispatched = std::make_shared<bool>(false);
        auto fire = [&, dispatched] {
            if (*dispatched)
                return;
            *dispatched = true;
            const int n = tools->getToolsDefinitions().size();
            err() << "[mcp] " << n << " tool(s) available.\n";
            dispatch();
        };
        QObject::connect(tools, &LLMQore::ToolRegistry::toolsChanged, &app, [&, fire] {
            if (!tools->getToolsDefinitions().isEmpty())
                fire();
        });
        QTimer::singleShot(15000, &app, fire);
    } else {
        QTimer::singleShot(0, &app, dispatch);
    }

    app.exec();
    return exitCode;
}
