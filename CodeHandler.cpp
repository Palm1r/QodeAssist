/*
 * Copyright (C) 2024 Petr Mironychev
 * Copyright (C) 2025 Povilas Kanapickas  <povilas@radix.lt>
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

#include "CodeHandler.hpp"
#include <QFileInfo>
#include <QHash>

namespace QodeAssist {

struct LanguageProperties
{
    QString name;
    QString commentStyle;
    QVector<QString> namesFromModel;
    QVector<QString> fileExtensions;
};

const QVector<LanguageProperties> &getKnownLanguages()
{
    static QVector<LanguageProperties> knownLanguages = {
        {"python", "#", {"python", "py"}, {"py"}},
        {"lua", "--", {"lua"}, {"lua"}},
        {"js", "//", {"js", "javascript"}, {"js", "jsx"}},
        {"ts", "//", {"ts", "typescript"}, {"ts", "tsx"}},
        {"c-like", "//", {"c", "c++", "cpp"}, {"c", "h", "cpp", "hpp"}},
        {"java", "//", {"java"}, {"java"}},
        {"c#", "//", {"cs", "csharp"}, {"cs"}},
        {"php", "//", {"php"}, {"php"}},
        {"ruby", "#", {"rb", "ruby"}, {"rb"}},
        {"go", "//", {"go"}, {"go"}},
        {"swift", "//", {"swift"}, {"swift"}},
        {"kotlin", "//", {"kt", "kotlin"}, {"kt", "kotlin"}},
        {"scala", "//", {"scala"}, {"scala"}},
        {"r", "#", {"r"}, {"r"}},
        {"shell", "#", {"shell", "bash", "sh"}, {"sh", "bash"}},
        {"perl", "#", {"pl", "perl"}, {"pl"}},
        {"hs", "--", {"hs", "haskell"}, {"hs"}},
        {"qml", "//", {"qml"}, {"qml"}},
    };

    return knownLanguages;
}

static QHash<QString, QString> buildLanguageToCommentPrefixMap()
{
    QHash<QString, QString> result;
    for (const auto &languageProps : getKnownLanguages()) {
        result[languageProps.name] = languageProps.commentStyle;
    }
    return result;
}

static QHash<QString, QString> buildExtensionToLanguageMap()
{
    QHash<QString, QString> result;
    for (const auto &languageProps : getKnownLanguages()) {
        for (const auto &extension : languageProps.fileExtensions) {
            result[extension] = languageProps.name;
        }
    }
    return result;
}

static QHash<QString, QString> buildModelLanguageNameToLanguageMap()
{
    QHash<QString, QString> result;
    for (const auto &languageProps : getKnownLanguages()) {
        for (const auto &nameFromModel : languageProps.namesFromModel) {
            result[nameFromModel] = languageProps.name;
        }
    }
    return result;
}

QString CodeHandler::processText(QString text, QString currentFilePath)
{
    QString result;
    QStringList lines = text.split('\n');
    bool inCodeBlock = false;
    QString pendingComments;

    auto currentFileExtension = QFileInfo(currentFilePath).suffix();
    auto currentLanguage = detectLanguageFromExtension(currentFileExtension);

    auto addPendingCommentsIfAny = [&]() {
        if (pendingComments.isEmpty()) {
            return;
        }
        QStringList commentLines = pendingComments.split('\n');
        QString commentPrefix = getCommentPrefix(currentLanguage);

        for (const QString &commentLine : commentLines) {
            if (!commentLine.trimmed().isEmpty()) {
                result += commentPrefix + " " + commentLine.trimmed() + "\n";
            } else {
                result += "\n";
            }
        }
        pendingComments.clear();
    };

    for (const QString &line : lines) {
        if (line.trimmed().startsWith("```")) {
            if (!inCodeBlock) {
                auto lineLanguage = detectLanguageFromLine(line);
                if (!lineLanguage.isEmpty()) {
                    currentLanguage = lineLanguage;
                }

                addPendingCommentsIfAny();

                if (lineLanguage.isEmpty()) {
                    // language not detected, so add direct output from model, if any
                    result += line.trimmed().mid(3) + "\n"; // add the remainder of line after ```
                }
            }
            inCodeBlock = !inCodeBlock;
            continue;
        }

        if (inCodeBlock) {
            result += line + "\n";
        } else {
            QString trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                pendingComments += trimmed + "\n";
            } else {
                pendingComments += "\n";
            }
        }
    }

    addPendingCommentsIfAny();

    return result;
}

QString CodeHandler::getCommentPrefix(const QString &language)
{
    static const auto commentPrefixes = buildLanguageToCommentPrefixMap();
    return commentPrefixes.value(language, "//");
}

QString CodeHandler::detectLanguageFromLine(const QString &line)
{
    static const auto modelNameToLanguage = buildModelLanguageNameToLanguageMap();
    return modelNameToLanguage.value(line.trimmed().mid(3).trimmed(), "");
}

QString CodeHandler::detectLanguageFromExtension(const QString &extension)
{
    static const auto extensionToLanguage = buildExtensionToLanguageMap();
    return extensionToLanguage.value(extension.toLower(), "");
}

const QRegularExpression &CodeHandler::getFullCodeBlockRegex()
{
    static const QRegularExpression
        regex(R"(```[\w\s]*\n([\s\S]*?)```)", QRegularExpression::MultilineOption);
    return regex;
}

const QRegularExpression &CodeHandler::getPartialStartBlockRegex()
{
    static const QRegularExpression
        regex(R"(```[\w\s]*\n([\s\S]*?)$)", QRegularExpression::MultilineOption);
    return regex;
}

const QRegularExpression &CodeHandler::getPartialEndBlockRegex()
{
    static const QRegularExpression regex(R"(^([\s\S]*?)```)", QRegularExpression::MultilineOption);
    return regex;
}

} // namespace QodeAssist
