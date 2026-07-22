// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "TurnContextTest.hpp"

#include <QTest>

#include "session/FencedText.hpp"
#include "session/Session.hpp"
#include "session/TurnContext.hpp"
#include "session/TurnContextBuilder.hpp"
#include "session/TurnContextPorts.hpp"

namespace QodeAssist {

namespace {

class FakeProjectContext : public Session::IProjectContextPort
{
public:
    explicit FakeProjectContext(Session::ProjectInfo info)
        : m_info(std::move(info))
    {}

    Session::ProjectInfo projectInfo() const override { return m_info; }

private:
    Session::ProjectInfo m_info;
};

class FakeSkillsContext : public Session::ISkillsContextPort
{
public:
    FakeSkillsContext(QString alwaysOn, QString catalog, QList<Session::InvokedSkill> skills)
        : m_alwaysOn(std::move(alwaysOn))
        , m_catalog(std::move(catalog))
        , m_skills(std::move(skills))
    {}

    QString alwaysOnBodies() const override { return m_alwaysOn; }
    QString catalogText() const override { return m_catalog; }

    std::optional<Session::InvokedSkill> findSkill(const QString &name) const override
    {
        for (const Session::InvokedSkill &skill : m_skills) {
            if (skill.name == name)
                return skill;
        }
        return std::nullopt;
    }

private:
    QString m_alwaysOn;
    QString m_catalog;
    QList<Session::InvokedSkill> m_skills;
};

Session::ProjectInfo sampleProject()
{
    Session::ProjectInfo info;
    info.available = true;
    info.displayName = "QodeAssist";
    info.sourceRoot = "/src/QodeAssist";
    info.buildDirectory = "/src/QodeAssist/build";
    return info;
}

} // namespace

void TurnContextTest::testTurnContextCollectsPartsFromPorts()
{
    FakeProjectContext projectPort(sampleProject());
    FakeSkillsContext skillsPort(
        "# Always on", "# Skills catalog", {Session::InvokedSkill{"review", "Review body"}});

    Session::TurnContextRequest request;
    request.message = "please /review this";
    request.basePrompt = "You are a helpful assistant.";

    const Session::TurnContext context
        = Session::TurnContextBuilder(projectPort, &skillsPort).build(request);

    QCOMPARE(context.basePrompt, QString("You are a helpful assistant."));
    QVERIFY(context.project.available);
    QCOMPARE(context.project.displayName, QString("QodeAssist"));
    QCOMPARE(context.alwaysOnSkills, QString("# Always on"));
    QCOMPARE(context.skillsCatalog, QString("# Skills catalog"));
    QCOMPARE(context.invokedSkills.size(), 1);
    QCOMPARE(context.invokedSkills.at(0).name, QString("review"));
}

void TurnContextTest::testTurnContextSkipsWhatTheBackendDoesNotNeed()
{
    FakeProjectContext projectPort(sampleProject());
    FakeSkillsContext skillsPort(
        "# Always on", "# Skills catalog", {Session::InvokedSkill{"review", "Review body"}});

    Session::TurnContextRequest request;
    request.message = "please /review this";
    request.needs = Session::TurnContextNeeds{false};

    const Session::TurnContext context
        = Session::TurnContextBuilder(projectPort, &skillsPort).build(request);

    QVERIFY(context.alwaysOnSkills.isEmpty());
    QVERIFY(context.skillsCatalog.isEmpty());
    QVERIFY(context.invokedSkills.isEmpty());
}

void TurnContextTest::testFencedFileBlockOutgrowsBackticksInContent()
{
    const QString plain = Session::fencedFileBlock("notes.txt", "hello");
    QCOMPARE(plain, QString("File: notes.txt\n```\nhello\n```"));

    const QString fenced = Session::fencedFileBlock("readme.md", "text\n```\ncode\n```\nmore");
    QVERIFY(fenced.startsWith("File: readme.md\n````\n"));
    QVERIFY(fenced.endsWith("\n````"));
    QVERIFY(fenced.contains("```\ncode\n```"));

    const QString nested = Session::fencedFileBlock("deep.md", "`````");
    QVERIFY(nested.contains("``````"));
}

void TurnContextTest::testTurnContextSkillCommandScanning()
{
    FakeProjectContext projectPort({});
    FakeSkillsContext skillsPort(
        QString(),
        QString(),
        {Session::InvokedSkill{"review", "Review body"},
         Session::InvokedSkill{"docs", "Docs body"},
         Session::InvokedSkill{"blank", QString()}});

    Session::TurnContextRequest request;
    request.message = "/review then /docs then /review again /blank /unknown and mid/word";

    const Session::TurnContext context
        = Session::TurnContextBuilder(projectPort, &skillsPort).build(request);

    QCOMPARE(context.invokedSkills.size(), 2);
    QCOMPARE(context.invokedSkills.at(0).name, QString("review"));
    QCOMPARE(context.invokedSkills.at(1).name, QString("docs"));
}

void TurnContextTest::testTurnContextWithoutSkillsPort()
{
    FakeProjectContext projectPort(sampleProject());

    Session::TurnContextRequest request;
    request.message = "/review this";
    request.basePrompt = "base";

    const Session::TurnContext context
        = Session::TurnContextBuilder(projectPort, nullptr).build(request);

    QVERIFY(context.alwaysOnSkills.isEmpty());
    QVERIFY(context.skillsCatalog.isEmpty());
    QVERIFY(context.invokedSkills.isEmpty());
}

void TurnContextTest::testSystemPromptRenderingWithProject()
{
    Session::TurnContext context;
    context.basePrompt = "You are helpful.";
    context.project = sampleProject();
    context.alwaysOnSkills = "# Always on";
    context.skillsCatalog = "# Catalog";
    context.invokedSkills = {Session::InvokedSkill{"review", "Review body"}};

    const QString expected
        = "You are helpful."
          "\n# Active project: QodeAssist"
          "\n# Project source root: /src/QodeAssist"
          "\n#   All new source files, headers, QML and CMake edits MUST be created or modified "
          "under this directory. Use absolute paths rooted here, or project-relative paths."
          "\n# Build output directory (compiler artifacts only — do NOT create or edit source "
          "files here): /src/QodeAssist/build"
          "\n\n# Always on"
          "\n\n# Catalog"
          "\n\n# Invoked Skill: review\n\nReview body";

    QCOMPARE(Session::renderSystemPrompt(context), expected);
}

void TurnContextTest::testSystemPromptRenderingWithoutProject()
{
    Session::TurnContext context;
    context.basePrompt = "You are helpful.";

    QCOMPARE(
        Session::renderSystemPrompt(context),
        QString("You are helpful.\n# No active project in IDE"));
}

} // namespace QodeAssist
