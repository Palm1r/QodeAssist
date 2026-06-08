// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "SkillsLoader.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSet>

namespace QodeAssist::Skills {

namespace {

QString unquote(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2
        && ((value.startsWith('"') && value.endsWith('"'))
            || (value.startsWith('\'') && value.endsWith('\'')))) {
        value = value.mid(1, value.size() - 2);
    }
    return value;
}

int indentOf(const QString &line)
{
    int i = 0;
    while (i < line.size() && line[i] == ' ')
        ++i;
    return i;
}

} // namespace

int SkillsLoader::maxBodyChars()
{
    return 64 * 1024;
}

bool SkillsLoader::parseFrontmatter(
    const QString &rawText, AgentSkill &skill, QString &body, QString &error)
{
    // Normalize line endings so CRLF/CR files parse identically to LF.
    QString text = rawText;
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    text.replace('\r', '\n');

    const QStringList lines = text.split('\n');
    if (lines.isEmpty() || lines.first().trimmed() != QLatin1String("---")) {
        error = QStringLiteral("missing YAML frontmatter");
        return false;
    }

    int closing = -1;
    for (int i = 1; i < lines.size(); ++i) {
        if (lines[i].trimmed() == QLatin1String("---")) {
            closing = i;
            break;
        }
    }
    if (closing < 0) {
        error = QStringLiteral("unterminated frontmatter");
        return false;
    }

    body = lines.mid(closing + 1).join('\n').trimmed();

    QHash<QString, QString> fields;
    int i = 1;
    while (i < closing) {
        const QString line = lines[i];
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#') || indentOf(line) != 0) {
            ++i;
            continue;
        }
        const int colon = line.indexOf(':');
        if (colon < 0) {
            ++i;
            continue;
        }
        const QString key = line.left(colon).trimmed();
        QString value = line.mid(colon + 1).trimmed();
        ++i;

        if (key == QLatin1String("metadata") && value.isEmpty()) {
            while (i < closing && (lines[i].trimmed().isEmpty() || indentOf(lines[i]) > 0)) {
                const QString entry = lines[i].trimmed();
                ++i;
                if (entry.isEmpty() || entry.startsWith('#'))
                    continue;
                const int entryColon = entry.indexOf(':');
                if (entryColon < 0)
                    continue;
                skill.metadata.insert(
                    entry.left(entryColon).trimmed(), unquote(entry.mid(entryColon + 1)));
            }
            continue;
        }

        if (value.startsWith('>') || value.startsWith('|')) {
            const bool literal = value.startsWith('|');
            QStringList block; // raw lines, indentation preserved
            while (i < closing && (lines[i].trimmed().isEmpty() || indentOf(lines[i]) > 0)) {
                block.append(lines[i]);
                ++i;
            }
            while (!block.isEmpty() && block.last().trimmed().isEmpty())
                block.removeLast();

            if (literal) {
                // Strip the common leading indentation, keep the rest verbatim.
                int common = -1;
                for (const QString &blockLine : block) {
                    if (blockLine.trimmed().isEmpty())
                        continue;
                    const int indent = indentOf(blockLine);
                    if (common < 0 || indent < common)
                        common = indent;
                }
                if (common < 0)
                    common = 0;
                QStringList stripped;
                for (const QString &blockLine : block)
                    stripped.append(blockLine.mid(qMin(common, blockLine.size())));
                value = stripped.join('\n');
            } else {
                // Folded scalar: join non-blank lines with single spaces.
                QStringList folded;
                for (const QString &blockLine : block) {
                    const QString trimmedLine = blockLine.trimmed();
                    if (!trimmedLine.isEmpty())
                        folded.append(trimmedLine);
                }
                value = folded.join(' ');
            }
            fields.insert(key, value);
            continue;
        }

        fields.insert(key, unquote(value));
    }

    skill.name = fields.value(QStringLiteral("name"));
    skill.description = fields.value(QStringLiteral("description"));
    skill.license = fields.value(QStringLiteral("license"));
    skill.compatibility = fields.value(QStringLiteral("compatibility"));
    const QString tools = fields.value(QStringLiteral("allowed-tools"));
    if (!tools.isEmpty())
        skill.allowedTools = tools.split(' ', Qt::SkipEmptyParts);
    return true;
}

bool SkillsLoader::validateName(const QString &name, const QString &dirName, QString &error)
{
    if (name.isEmpty()) {
        error = QStringLiteral("missing 'name'");
        return false;
    }
    if (name.size() > 64) {
        error = QStringLiteral("'name' exceeds 64 characters");
        return false;
    }
    for (const QChar c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
        if (!ok) {
            error = QStringLiteral(
                "'name' may only contain lowercase letters, digits and hyphens");
            return false;
        }
    }
    if (name.startsWith('-') || name.endsWith('-')) {
        error = QStringLiteral("'name' must not start or end with a hyphen");
        return false;
    }
    if (name.contains(QLatin1String("--"))) {
        error = QStringLiteral("'name' must not contain consecutive hyphens");
        return false;
    }
    // The directory name may differ in case on case-insensitive filesystems
    // (macOS, Windows); the spec only requires the names to match.
    if (name.compare(dirName, Qt::CaseInsensitive) != 0) {
        error = QStringLiteral("'name' (%1) must match the skill directory name (%2)")
                    .arg(name, dirName);
        return false;
    }
    return true;
}

SkillsLoader::ParseResult SkillsLoader::parseSkillFile(
    const QString &skillDir, const QString &rootPath)
{
    ParseResult result;

    const QString skillMdPath = QDir(skillDir).absoluteFilePath(QStringLiteral("SKILL.md"));
    QFile file(skillMdPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = QStringLiteral("cannot open SKILL.md");
        return result;
    }
    const QString text = QString::fromUtf8(file.readAll());
    file.close();

    AgentSkill skill;
    QString body;
    if (!parseFrontmatter(text, skill, body, result.error))
        return result;

    const QString dirName = QDir(skillDir).dirName();
    if (!validateName(skill.name, dirName, result.error))
        return result;

    if (skill.description.isEmpty()) {
        result.error = QStringLiteral("missing 'description'");
        return result;
    }
    if (skill.description.size() > 1024) {
        result.error = QStringLiteral("'description' exceeds 1024 characters");
        return result;
    }

    skill.alwaysOn = skill.metadata.value(QStringLiteral("always-on"))
                         .compare(QLatin1String("true"), Qt::CaseInsensitive)
                     == 0;
    if (body.size() > maxBodyChars()) {
        body.truncate(maxBodyChars());
        body += QStringLiteral("\n\n[skill body truncated]");
    }
    skill.body = body;
    skill.skillDir = QDir(skillDir).absolutePath();
    skill.rootPath = rootPath;
    result.skill = skill;
    result.valid = true;
    return result;
}

QVector<AgentSkill> SkillsLoader::scan(const QStringList &rootPaths)
{
    QVector<AgentSkill> skills;
    QSet<QString> seenNames;

    for (const QString &root : rootPaths) {
        QDir rootDir(root);
        if (!rootDir.exists())
            continue;

        const QStringList entries = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &entry : entries) {
            const QString skillDir = rootDir.absoluteFilePath(entry);
            if (!QFile::exists(QDir(skillDir).absoluteFilePath(QStringLiteral("SKILL.md"))))
                continue;

            const ParseResult result = parseSkillFile(skillDir, root);
            if (!result.valid) {
                qWarning().noquote()
                    << "QodeAssist Skills: skipping" << skillDir << "-" << result.error;
                continue;
            }
            if (seenNames.contains(result.skill.name))
                continue; // earlier root wins
            seenNames.insert(result.skill.name);
            skills.append(result.skill);
        }
    }
    return skills;
}

} // namespace QodeAssist::Skills
