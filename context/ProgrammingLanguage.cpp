// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProgrammingLanguage.hpp"

namespace QodeAssist::Context {

ProgrammingLanguage ProgrammingLanguageUtils::fromMimeType(const QString &mimeType)
{
    if (mimeType == "text/x-qml" || mimeType == "application/javascript"
        || mimeType == "text/javascript" || mimeType == "text/x-javascript") {
        return ProgrammingLanguage::QML;
    }
    if (mimeType == "text/x-c++src" || mimeType == "text/x-c++hdr" || mimeType == "text/x-csrc"
        || mimeType == "text/x-chdr") {
        return ProgrammingLanguage::Cpp;
    }
    if (mimeType == "text/x-python") {
        return ProgrammingLanguage::Python;
    }
    return ProgrammingLanguage::Unknown;
}

QString ProgrammingLanguageUtils::toString(ProgrammingLanguage language)
{
    switch (language) {
    case ProgrammingLanguage::Cpp:
        return "c/c++";
    case ProgrammingLanguage::QML:
        return "qml";
    case ProgrammingLanguage::Python:
        return "python";
    case ProgrammingLanguage::Unknown:
    default:
        return QString();
    }
}

ProgrammingLanguage ProgrammingLanguageUtils::fromString(const QString &str)
{
    QString lower = str.toLower();
    if (lower == "c/c++") {
        return ProgrammingLanguage::Cpp;
    }
    if (lower == "qml") {
        return ProgrammingLanguage::QML;
    }
    if (lower == "python") {
        return ProgrammingLanguage::Python;
    }
    return ProgrammingLanguage::Unknown;
}

} // namespace QodeAssist::Context
