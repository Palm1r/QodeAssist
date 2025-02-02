/* 
 * Copyright (C) 2024 Petr Mironychev
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
