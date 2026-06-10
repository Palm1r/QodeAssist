// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>
#include <QTimer>

#include <memory>
#include <vector>

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/ContentBlocks.hpp>
#include <LLMQore/ToolRegistry.hpp>
#include <LLMQore/ToolsManager.hpp>

#include <Agent.hpp>
#include <AgentConfig.hpp>
#include <AgentFactory.hpp>
#include <ContextData.hpp>
#include <ContextRenderer.hpp>
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
        if (const auto *d = ev.as<ResponseEvents::Usage>())
            err() << "\n[usage] in=" << d->inputTokens << " out=" << d->outputTokens << "\n";
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
        "Prompt text. If omitted, positional args or stdin are used.",
        "text");
    QCommandLineOption noThinkingOpt("no-thinking", "Hide thinking deltas from output.");
    QCommandLineOption envOpt(
        QStringList{"e", "env"},
        "Read API keys from a dotenv file (KEY=VALUE per line). Defaults to ./.env if present.",
        "path");
    QCommandLineOption apiKeyOpt(
        "api-key", "API key to use for the agent's provider (overrides env/settings).", "value");
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
    parser.addOption(projectDirOpt);
    parser.addOption(imageOpt);
    parser.addOption(mcpOpt);
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

    QString prompt = parser.value(promptOpt);
    if (prompt.isEmpty())
        prompt = parser.positionalArguments().join(QLatin1Char(' '));
    if (prompt.isEmpty() && imagePaths.isEmpty())
        prompt = readStdin().trimmed();
    if (prompt.isEmpty() && imagePaths.isEmpty()) {
        err() << "Empty prompt.\n";
        return 2;
    }

    if (!imagePaths.isEmpty() && !session->supportsImages())
        err() << "[warning] agent's provider does not advertise image support.\n";

    const bool showThinking = !parser.isSet(noThinkingOpt);
    int exitCode = 0;

    QObject::connect(
        session, &Session::event, &app, [showThinking](const ResponseEvent &ev) {
            printEvent(ev, showThinking);
        });
    QObject::connect(
        session, &Session::finished, &app,
        [&](const LLMQore::RequestID &, const QString &reason) {
            err() << "\n[done] stopReason=" << (reason.isEmpty() ? "<none>" : reason) << "\n";
            QCoreApplication::quit();
        });
    QObject::connect(
        session, &Session::failed, &app,
        [&](const LLMQore::RequestID &, const QString &msg) {
            err() << "\n[failed] " << msg << "\n";
            exitCode = 1;
            QCoreApplication::quit();
        });

    auto dispatch = [&] {
        if (fimMode) {
            Templates::ContextData ctx;
            ctx.prefix = prompt;
            if (parser.isSet(suffixOpt))
                ctx.suffix = parser.value(suffixOpt);
            if (session->sendCompletion(std::move(ctx)).isEmpty()) {
                err() << "Failed to dispatch FIM request (check provider URL / model).\n";
                exitCode = 1;
                QCoreApplication::quit();
            }
            return;
        }

        std::vector<std::unique_ptr<LLMQore::ContentBlock>> blocks;
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
        if (!prompt.isEmpty())
            blocks.push_back(std::make_unique<LLMQore::TextContent>(prompt));
        if (blocks.empty()) {
            err() << "Nothing to send.\n";
            exitCode = 1;
            QCoreApplication::quit();
            return;
        }
        if (session->send(std::move(blocks)).isEmpty()) {
            err() << "Failed to dispatch request (check provider URL / model).\n";
            exitCode = 1;
            QCoreApplication::quit();
        }
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
