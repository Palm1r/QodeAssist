// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "CompletionContextEnricher.hpp"

#include <QRegularExpression>
#include <QSet>

#include <cplusplus/Literals.h>
#include <cplusplus/Names.h>
#include <cplusplus/Overview.h>
#include <cplusplus/Scope.h>
#include <cplusplus/Symbols.h>
#include <cppeditor/cppmodelmanager.h>
#include <qmljs/qmljsdocument.h>
#include <qmljs/qmljsmodelmanagerinterface.h>
#include <utils/filepath.h>

#include "ProgrammingLanguage.hpp"
#include "TokenUtils.hpp"

namespace QodeAssist::Context {

namespace {

constexpr int kMaxPrefixIdentifiers = 40;
constexpr int kMaxEnclosingMembers = 40;
constexpr int kMaxReferencedDeclarations = 60;
constexpr int kMaxIncludedDocuments = 30;
constexpr int kMaxSiblingComponents = 60;
constexpr int kMaxVisitedSymbols = 5000;
constexpr int kPrefixScanWindow = 4096;

const QSet<QString> &cppKeywords()
{
    static const QSet<QString> keywords = {
        "alignas",   "alignof",  "asm",       "auto",      "bool",         "break",
        "case",      "catch",    "char",      "class",     "const",        "constexpr",
        "consteval", "constinit","continue",  "decltype",  "default",      "delete",
        "do",        "double",   "else",      "enum",      "explicit",     "export",
        "extern",    "false",    "float",     "for",       "friend",       "goto",
        "if",        "inline",   "int",       "long",      "mutable",      "namespace",
        "new",       "noexcept", "nullptr",   "operator",  "private",      "protected",
        "public",    "register", "requires",  "return",    "short",        "signed",
        "sizeof",    "static",   "struct",    "switch",    "template",     "this",
        "throw",     "true",     "try",       "typedef",   "typeid",       "typename",
        "union",     "unsigned", "using",     "virtual",   "void",         "volatile",
        "while",     "override", "final",     "co_await",  "co_return",    "co_yield"};
    return keywords;
}

QString prettySymbol(const CPlusPlus::Overview &overview, CPlusPlus::Symbol *symbol)
{
    return overview.prettyType(symbol->type(), overview.prettyName(symbol->name()));
}

QString cppEnclosingScopeSection(
    const CPlusPlus::Document::Ptr &document,
    const CPlusPlus::Overview &overview,
    int line,
    int column)
{
    CPlusPlus::Scope *scope = document->scopeAt(line + 1, column + 1);
    for (CPlusPlus::Scope *current = scope; current; current = current->enclosingScope()) {
        CPlusPlus::Class *klass = current->asClass();
        if (!klass || !klass->name())
            continue;

        QStringList members;
        const int memberCount = qMin(klass->memberCount(), kMaxEnclosingMembers);
        for (int i = 0; i < memberCount; ++i) {
            CPlusPlus::Symbol *member = klass->memberAt(i);
            if (!member || !member->name())
                continue;
            members.append("  " + prettySymbol(overview, member));
        }

        if (members.isEmpty())
            continue;

        return QString("Enclosing class %1:\n%2")
            .arg(overview.prettyName(klass->name()), members.join('\n'));
    }
    return {};
}

QString cppReferencedDeclarationsSection(
    const CPlusPlus::Snapshot &snapshot,
    const CPlusPlus::Document::Ptr &document,
    const CPlusPlus::Overview &overview,
    const QString &prefix)
{
    const QStringList identifiers
        = identifiersNearCursor(prefix.right(kPrefixScanWindow), kMaxPrefixIdentifiers);
    if (identifiers.isEmpty())
        return {};

    QSet<QByteArray> wanted;
    for (const QString &identifier : identifiers)
        wanted.insert(identifier.toUtf8());

    QList<CPlusPlus::Document::Ptr> documents{document};
    const Utils::FilePaths includes = document->includedFiles();
    for (const Utils::FilePath &include : includes) {
        if (documents.size() > kMaxIncludedDocuments)
            break;
        if (auto included = snapshot.document(include))
            documents.append(included);
    }

    QStringList declarations;
    QSet<QString> seen;
    int visited = 0;

    auto collect = [&](auto self, CPlusPlus::Scope *scope) -> void {
        if (!scope || declarations.size() >= kMaxReferencedDeclarations
            || visited >= kMaxVisitedSymbols)
            return;
        for (int i = 0; i < scope->memberCount(); ++i) {
            if (declarations.size() >= kMaxReferencedDeclarations
                || ++visited >= kMaxVisitedSymbols)
                return;
            CPlusPlus::Symbol *symbol = scope->memberAt(i);
            if (!symbol || !symbol->name())
                continue;
            if (const CPlusPlus::Identifier *nameId = symbol->name()->asNameId()) {
                const QByteArray key
                    = QByteArray::fromRawData(nameId->chars(), int(nameId->size()));
                if (wanted.contains(key)) {
                    const QString pretty = "  " + prettySymbol(overview, symbol);
                    if (!seen.contains(pretty)) {
                        seen.insert(pretty);
                        declarations.append(pretty);
                    }
                }
            }
            if (symbol->asFunction() || symbol->asBlock())
                continue;
            if (auto nested = symbol->asScope())
                self(self, nested);
        }
    };

    for (const auto &doc : std::as_const(documents)) {
        if (doc->globalNamespace())
            collect(collect, doc->globalNamespace());
    }

    if (declarations.isEmpty())
        return {};

    return QString("Declarations referenced near the cursor:\n%1").arg(declarations.join('\n'));
}

QStringList cppEnrichment(const QString &filePath, const QString &prefix, int line, int column)
{
    auto *modelManager = CppEditor::CppModelManager::instance();
    if (!modelManager)
        return {};

    const CPlusPlus::Snapshot snapshot = modelManager->snapshot();
    const CPlusPlus::Document::Ptr document
        = snapshot.document(Utils::FilePath::fromString(filePath));
    if (!document || !document->globalNamespace())
        return {};

    CPlusPlus::Overview overview;
    overview.showReturnTypes = true;
    overview.showArgumentNames = true;

    QStringList sections;
    const QString enclosing = cppEnclosingScopeSection(document, overview, line, column);
    if (!enclosing.isEmpty())
        sections.append(enclosing);
    const QString referenced
        = cppReferencedDeclarationsSection(snapshot, document, overview, prefix);
    if (!referenced.isEmpty())
        sections.append(referenced);
    return sections;
}

QStringList qmlEnrichment(const QString &filePath)
{
    auto *modelManager = QmlJS::ModelManagerInterface::instance();
    if (!modelManager)
        return {};

    const QmlJS::Snapshot snapshot = modelManager->snapshot();
    const Utils::FilePath path = Utils::FilePath::fromString(filePath);
    const QmlJS::Document::Ptr document = snapshot.document(path);
    const Utils::FilePath directory = document ? document->path() : path.parentDir();

    QStringList components;
    const QList<QmlJS::Document::Ptr> siblings = snapshot.documentsInDirectory(directory);
    for (const auto &sibling : siblings) {
        if (components.size() >= kMaxSiblingComponents)
            break;
        if (!sibling || sibling->fileName() == path)
            continue;
        const QString component = sibling->componentName();
        if (!component.isEmpty() && !components.contains(component))
            components.append(component);
    }

    if (components.isEmpty())
        return {};

    components.sort();
    return QStringList{
        QString("QML components available in this directory: %1").arg(components.join(", "))};
}

} // anonymous namespace

QString SemanticContextEnricher::enrichmentFor(
    const DocumentInfo &documentInfo, const QString &prefix, int line, int column, int maxTokens)
{
    const ProgrammingLanguage language
        = ProgrammingLanguageUtils::fromMimeType(documentInfo.mimeType);

    QStringList sections;
    switch (language) {
    case ProgrammingLanguage::Cpp:
        sections = cppEnrichment(documentInfo.filePath, prefix, line, column);
        break;
    case ProgrammingLanguage::QML:
        sections = qmlEnrichment(documentInfo.filePath);
        break;
    default:
        return {};
    }

    const QString clamped = clampSectionsToTokenBudget(sections, maxTokens);
    if (clamped.isEmpty())
        return {};

    return QString("\n\n# Project code model context (may lag unsaved edits)\n%1\n").arg(clamped);
}

QString clampSectionsToTokenBudget(const QStringList &sections, int maxTokens)
{
    QStringList kept;
    int remaining = maxTokens;
    for (const QString &section : sections) {
        if (section.isEmpty())
            continue;
        const int cost = TokenUtils::estimateTokens(section);
        if (cost > remaining)
            continue;
        kept.append(section);
        remaining -= cost;
    }
    return kept.join("\n\n");
}

QStringList identifiersNearCursor(const QString &prefix, int maxIdentifiers)
{
    static const QRegularExpression identifierRegex(
        QStringLiteral("[A-Za-z_][A-Za-z0-9_]{2,}"));

    QStringList result;
    QSet<QString> seen;

    QRegularExpressionMatchIterator it = identifierRegex.globalMatch(prefix);
    QStringList all;
    while (it.hasNext())
        all.append(it.next().captured());

    for (auto identifier = all.crbegin(); identifier != all.crend(); ++identifier) {
        if (result.size() >= maxIdentifiers)
            break;
        if (seen.contains(*identifier) || cppKeywords().contains(*identifier))
            continue;
        seen.insert(*identifier);
        result.append(*identifier);
    }

    return result;
}

} // namespace QodeAssist::Context
