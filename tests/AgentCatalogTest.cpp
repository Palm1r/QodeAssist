// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentCatalogTest.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "ChatView/ChatConfigurationController.hpp"
#include "acp/AgentCatalog.hpp"
#include "acp/AgentCatalogStore.hpp"
#include "acp/AgentDefinition.hpp"
#include "acp/AgentLaunch.hpp"
#include "acp/AgentRegistryParser.hpp"
#include "acp/AgentSpawn.hpp"

namespace QodeAssist {

void AgentCatalogTest::testAgentRegistryParsesEveryDistributionKind()
{
    const QByteArray payload = R"({"version":"1.0.0","agents":[
        {"id":"npx-agent","name":"Npx Agent","version":"1.2.3","description":"An agent",
         "authors":["Ada","Grace"],"license":"MIT","repository":"https://example.com/repo",
         "website":"https://example.com","icon":"https://example.com/icon.svg",
         "distribution":{"npx":{"package":"pkg@1.2.3","args":["--acp"],"env":{"KEY":"VALUE"}}}},
        {"id":"uvx-agent","name":"Uvx Agent",
         "distribution":{"uvx":{"package":"pypkg==1.0","args":["-x"]}}},
        {"id":"binary-agent","name":"Binary Agent","distribution":{"binary":{
            "darwin-aarch64":{"archive":"https://example.com/a.tar.gz","sha256":"abc",
                              "cmd":"./dist/agent","args":["acp"]}}}},
        {"id":"command-agent","name":"Command Agent",
         "distribution":{"command":{"cmd":"/usr/local/bin/agent","args":["acp"]}}}
    ]})";

    const Acp::AgentParseResult result = Acp::AgentRegistryParser::parse(
        payload, Acp::AgentSource::LiveRegistry, "registry");

    QVERIFY(result.warnings.isEmpty());
    QCOMPARE(result.agents.size(), 4);

    const Acp::AgentDefinition &npx = result.agents.at(0);
    QCOMPARE(npx.id, QString("npx-agent"));
    QCOMPARE(npx.name, QString("Npx Agent"));
    QCOMPARE(npx.version, QString("1.2.3"));
    QCOMPARE(npx.description, QString("An agent"));
    QCOMPARE(npx.authors, (QStringList{"Ada", "Grace"}));
    QCOMPARE(npx.license, QString("MIT"));
    QCOMPARE(npx.repository, QString("https://example.com/repo"));
    QCOMPARE(npx.source, Acp::AgentSource::LiveRegistry);
    QCOMPARE(npx.origin, QString("registry"));
    QCOMPARE(npx.distribution.kind, Acp::AgentDistributionKind::Npx);
    QCOMPARE(npx.distribution.package, QString("pkg@1.2.3"));
    QCOMPARE(npx.distribution.args, (QStringList{"--acp"}));
    QCOMPARE(npx.distribution.env.size(), 1);
    QCOMPARE(npx.distribution.env.at(0).name, QString("KEY"));
    QCOMPARE(npx.distribution.env.at(0).value, QString("VALUE"));
    QVERIFY(npx.isLaunchable());

    const Acp::AgentDefinition &uvx = result.agents.at(1);
    QCOMPARE(uvx.distribution.kind, Acp::AgentDistributionKind::Uvx);
    QCOMPARE(uvx.distribution.package, QString("pypkg==1.0"));
    QVERIFY(uvx.isLaunchable());

    const Acp::AgentDefinition &binary = result.agents.at(2);
    QCOMPARE(binary.distribution.kind, Acp::AgentDistributionKind::Binary);
    QCOMPARE(binary.distribution.binaryTargets.size(), 1);
    QCOMPARE(binary.distribution.binaryTargets.at(0).platform, QString("darwin-aarch64"));
    QCOMPARE(binary.distribution.binaryTargets.at(0).cmd, QString("./dist/agent"));
    QCOMPARE(binary.distribution.binaryTargets.at(0).sha256, QString("abc"));
    QVERIFY(!binary.isLaunchable());

    const Acp::AgentDefinition &command = result.agents.at(3);
    QCOMPARE(command.distribution.kind, Acp::AgentDistributionKind::Command);
    QCOMPARE(command.distribution.command, QString("/usr/local/bin/agent"));
    QVERIFY(command.isLaunchable());
}

void AgentCatalogTest::testAgentRegistryReportsUnusableEntries()
{
    const QByteArray payload = R"({"agents":[
        {"name":"No Id","distribution":{"npx":{"package":"p"}}},
        {"id":"no-distribution","name":"No Distribution"},
        {"id":"usable","name":"Usable","distribution":{"npx":{"package":"p"}}}
    ]})";

    const Acp::AgentParseResult result = Acp::AgentRegistryParser::parse(
        payload, Acp::AgentSource::LiveRegistry, "registry");

    QCOMPARE(result.agents.size(), 2);
    QCOMPARE(result.warnings.size(), 2);
    QVERIFY(result.warnings.at(0).contains("entry 0"));
    QVERIFY(result.warnings.at(1).contains("no-distribution"));

    QCOMPARE(result.agents.at(0).id, QString("no-distribution"));
    QVERIFY(!result.agents.at(0).isLaunchable());
    QVERIFY(result.agents.at(1).isLaunchable());
}

void AgentCatalogTest::testAgentRegistryParsesSingleAgentUserFile()
{
    const QByteArray payload
        = R"({"id":"local","distribution":{"command":{"cmd":"/opt/local-agent"}}})";

    const Acp::AgentParseResult result = Acp::AgentRegistryParser::parse(
        payload, Acp::AgentSource::UserFile, "local.json");

    QCOMPARE(result.agents.size(), 1);
    QCOMPARE(result.agents.at(0).id, QString("local"));
    QCOMPARE(result.agents.at(0).name, QString("local"));
    QCOMPARE(result.agents.at(0).source, Acp::AgentSource::UserFile);
    QVERIFY(result.agents.at(0).isLaunchable());
}

void AgentCatalogTest::testAgentCatalogAppliesMergePriority()
{
    const QByteArray bundled = R"({"agents":[
        {"id":"alpha","name":"Alpha Bundled","distribution":{"npx":{"package":"alpha@1"}}},
        {"id":"beta","name":"Beta","distribution":{"npx":{"package":"beta@1"}}}
    ]})";
    const QByteArray live = R"({"agents":[
        {"id":"alpha","name":"Alpha Registry","distribution":{"npx":{"package":"alpha@2"}}},
        {"id":"gamma","name":"Gamma","distribution":{"uvx":{"package":"gamma"}}}
    ]})";
    const QByteArray user
        = R"({"id":"alpha","name":"Alpha User","distribution":{"command":{"cmd":"/opt/alpha"}}})";

    Acp::AgentCatalog catalog;
    catalog.setLayer(
        Acp::AgentSource::BundledSnapshot,
        Acp::AgentRegistryParser::parse(bundled, Acp::AgentSource::BundledSnapshot, "bundled")
            .agents);
    catalog.setLayer(
        Acp::AgentSource::LiveRegistry,
        Acp::AgentRegistryParser::parse(live, Acp::AgentSource::LiveRegistry, "registry").agents);
    catalog.setLayer(
        Acp::AgentSource::UserFile,
        Acp::AgentRegistryParser::parse(user, Acp::AgentSource::UserFile, "alpha.json").agents);

    QCOMPARE(catalog.size(), 3);

    QStringList names;
    for (const Acp::AgentDefinition &agent : catalog.agents())
        names.append(agent.name);
    QCOMPARE(names, (QStringList{"Alpha User", "Beta", "Gamma"}));

    auto alpha = catalog.agent("alpha");
    QVERIFY(alpha.has_value());
    QCOMPARE(alpha->source, Acp::AgentSource::UserFile);
    QCOMPARE(alpha->distribution.kind, Acp::AgentDistributionKind::Command);

    catalog.setLayer(Acp::AgentSource::UserFile, {});
    alpha = catalog.agent("alpha");
    QVERIFY(alpha.has_value());
    QCOMPARE(alpha->name, QString("Alpha Registry"));
    QCOMPARE(alpha->distribution.package, QString("alpha@2"));

    catalog.setLayer(Acp::AgentSource::LiveRegistry, {});
    alpha = catalog.agent("alpha");
    QVERIFY(alpha.has_value());
    QCOMPARE(alpha->name, QString("Alpha Bundled"));
}

void AgentCatalogTest::testAgentCatalogUserFileMakesBinaryAgentLaunchable()
{
    const QByteArray live = R"({"agents":[{"id":"cursor","name":"Cursor",
        "distribution":{"binary":{"darwin-aarch64":{"archive":"https://example.com/a.tar.gz",
                                                    "cmd":"./cursor-agent","args":["acp"]}}}}]})";
    const QByteArray user = R"({"id":"cursor","name":"Cursor",
        "distribution":{"command":{"cmd":"/usr/local/bin/cursor-agent","args":["acp"]}}})";

    Acp::AgentCatalog catalog;
    catalog.setLayer(
        Acp::AgentSource::LiveRegistry,
        Acp::AgentRegistryParser::parse(live, Acp::AgentSource::LiveRegistry, "registry").agents);
    QVERIFY(catalog.launchableAgents().isEmpty());

    catalog.setLayer(
        Acp::AgentSource::UserFile,
        Acp::AgentRegistryParser::parse(user, Acp::AgentSource::UserFile, "cursor.json").agents);

    QCOMPARE(catalog.size(), 1);
    QCOMPARE(catalog.launchableAgents().size(), 1);
    QCOMPARE(catalog.agent("cursor")->distribution.command, QString("/usr/local/bin/cursor-agent"));
}

void AgentCatalogTest::testAgentLaunchConfigUsesRunnerConventions()
{
    Acp::AgentDefinition npx;
    npx.id = "npx-agent";
    npx.distribution.kind = Acp::AgentDistributionKind::Npx;
    npx.distribution.package = "pkg@1.0";
    npx.distribution.args = QStringList{"--acp"};
    npx.distribution.env = {{"KEY", "VALUE"}};

    const auto npxConfig = Acp::agentLaunchConfig(npx, "/tmp/project");
    QVERIFY(npxConfig.has_value());
    QCOMPARE(npxConfig->cwd, QString("/tmp/project"));
    QCOMPARE(npxConfig->command, QString("npx"));
    QCOMPARE(npxConfig->args, (QStringList{"-y", "pkg@1.0", "--acp"}));
    QCOMPARE(npxConfig->env.size(), 1);
    QCOMPARE(npxConfig->env.at(0).name, QString("KEY"));

    Acp::AgentDefinition uvx;
    uvx.id = "uvx-agent";
    uvx.distribution.kind = Acp::AgentDistributionKind::Uvx;
    uvx.distribution.package = "pypkg==1.0";
    uvx.distribution.args = QStringList{"-x"};

    const auto uvxConfig = Acp::agentLaunchConfig(uvx, QString());
    QVERIFY(uvxConfig.has_value());
    QCOMPARE(uvxConfig->command, QString("uvx"));
    QCOMPARE(uvxConfig->args, (QStringList{"pypkg==1.0", "-x"}));

    Acp::AgentDefinition command;
    command.id = "command-agent";
    command.distribution.kind = Acp::AgentDistributionKind::Command;
    command.distribution.command = "/opt/agent";
    command.distribution.args = QStringList{"acp"};

    const auto commandConfig = Acp::agentLaunchConfig(command, QString());
    QVERIFY(commandConfig.has_value());
    QCOMPARE(commandConfig->command, QString("/opt/agent"));
    QCOMPARE(commandConfig->args, (QStringList{"acp"}));

    Acp::AgentDefinition binary;
    binary.id = "binary-agent";
    binary.distribution.kind = Acp::AgentDistributionKind::Binary;
    QVERIFY(!Acp::agentLaunchConfig(binary, QString()).has_value());
}

void AgentCatalogTest::testAgentSearchPathsSplitting()
{
    const QString separator(QDir::listSeparator());

    QCOMPARE(Acp::splitSearchPaths(QString()), QStringList());
    QCOMPARE(
        Acp::splitSearchPaths(" /opt/homebrew/bin " + separator + separator + "/usr/local/bin"),
        (QStringList{"/opt/homebrew/bin", "/usr/local/bin"}));
}

void AgentCatalogTest::testAgentExtraSearchPathsReachTheChildEnvironment()
{
    Acp::AgentDefinition agent;
    agent.id = "npx-agent";
    agent.distribution.kind = Acp::AgentDistributionKind::Npx;
    agent.distribution.package = "pkg";

    auto config = Acp::agentLaunchConfig(agent, QString());
    QVERIFY(config.has_value());

    Acp::applyExtraSearchPaths(*config, {});
    QVERIFY(config->env.isEmpty());
    QCOMPARE(config->command, QString("npx"));

    Acp::applyExtraSearchPaths(*config, QStringList{"/no/such/qodeassist/dir"});
    QCOMPARE(config->command, QString("npx"));
    QCOMPARE(config->env.size(), 1);
    QCOMPARE(config->env.at(0).name, QString("PATH"));
    QVERIFY(config->env.at(0).value.startsWith("/no/such/qodeassist/dir"));

    Acp::AgentDefinition withOwnPath;
    withOwnPath.id = "own-path";
    withOwnPath.distribution.kind = Acp::AgentDistributionKind::Npx;
    withOwnPath.distribution.package = "pkg";
    withOwnPath.distribution.env = {{"PATH", "/agent/own"}};

    auto ownPathConfig = Acp::agentLaunchConfig(withOwnPath, QString());
    QVERIFY(ownPathConfig.has_value());
    Acp::applyExtraSearchPaths(*ownPathConfig, QStringList{"/extra"});

    QCOMPARE(ownPathConfig->env.size(), 2);
    QCOMPARE(ownPathConfig->env.at(0).name, QString("PATH"));
    QCOMPARE(ownPathConfig->env.at(1).value, QString("/agent/own"));
}

void AgentCatalogTest::testAgentForwardedVariablesReachTheChildEnvironment()
{
    QCOMPARE(
        Acp::splitVariableNames(" FOO, BAR\tBAZ "), (QStringList{"FOO", "BAR", "BAZ"}));
    QCOMPARE(Acp::splitVariableNames(QString()), QStringList());

    qputenv("QODEASSIST_TEST_TOKEN", "from-environment");

    Acp::AgentDefinition agent;
    agent.id = "npx-agent";
    agent.distribution.kind = Acp::AgentDistributionKind::Npx;
    agent.distribution.package = "pkg";
    agent.distribution.env = {{"QODEASSIST_TEST_TOKEN", "from-definition"}};

    auto config = Acp::agentLaunchConfig(agent, QString());
    QVERIFY(config.has_value());

    Acp::applyForwardedEnvironment(*config, {});
    QCOMPARE(config->env.size(), 1);

    Acp::applyForwardedEnvironment(*config, QStringList{"QODEASSIST_TEST_TOKEN"});

    QCOMPARE(config->env.size(), 2);
    QCOMPARE(config->env.at(0).name, QString("QODEASSIST_TEST_TOKEN"));
    QCOMPARE(config->env.at(0).value, QString("from-environment"));
    QCOMPARE(config->env.at(1).value, QString("from-definition"));

    qunsetenv("QODEASSIST_TEST_TOKEN");
}

void AgentCatalogTest::testBundledAgentSnapshotParses()
{
    QFile file(Acp::AgentCatalogStore::bundledSnapshotPath());
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.fileName()));

    const Acp::AgentParseResult result = Acp::AgentRegistryParser::parse(
        file.readAll(), Acp::AgentSource::BundledSnapshot, "bundled snapshot");

    QVERIFY(result.agents.size() > 10);
    QVERIFY2(result.warnings.isEmpty(), qPrintable(result.warnings.join("; ")));

    Acp::AgentCatalog catalog;
    catalog.setLayer(Acp::AgentSource::BundledSnapshot, result.agents);
    QVERIFY(!catalog.launchableAgents().isEmpty());
}

void AgentCatalogTest::testPickerObservesTheSharedAgentCatalog()
{
    Acp::AgentCatalogStore store;
    Chat::ChatConfigurationController controller;

    QSignalSpy listChanged(
        &controller, &Chat::ChatConfigurationController::availableConfigurationsChanged);

    controller.setAgentCatalog(&store);
    QVERIFY(listChanged.count() >= 1);
    QVERIFY(!controller.availableConfigurations().isEmpty());

    const int before = listChanged.count();
    emit store.catalogChanged();
    QCOMPARE(listChanged.count(), before + 1);

    controller.setAgentCatalog(&store);
    QCOMPARE(listChanged.count(), before + 1);
}

} // namespace QodeAssist
