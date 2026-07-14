// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>

namespace QodeAssist::Context {

enum class ProgrammingLanguage {
    QML, // QML/JavaScript
    Cpp, // C/C++
    Python,
    Unknown,
};

namespace ProgrammingLanguageUtils {

ProgrammingLanguage fromMimeType(const QString &mimeType);

QString toString(ProgrammingLanguage language);

ProgrammingLanguage fromString(const QString &str);

} // namespace ProgrammingLanguageUtils

} // namespace QodeAssist::Context
