// Copyright (C) 2026 Petr Mironychev
// Copyright (C) 2025 Povilas Kanapickas <povilas@radix.lt>
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QSharedPointer>
#include <QString>

#include "context/DocumentContextReader.hpp"
#include "settings/CodeCompletionSettings.hpp"

namespace QodeAssist {

class DocumentContextReaderTest final : public QObject
{
    Q_OBJECT

private slots:
    void testGetLineText();
    void testGetContext();
    void testGetContextWithCopyright();
    void testReadWholeFile();
    void testReadWholeFileWithCopyright();
    void testReadWholeFileWithMultilineCopyright();
    void testFindCopyrightSingleLine();
    void testFindCopyrightMultiLine();
    void testFindCopyrightMultipleBlocks();
    void testFindCopyrightNoCopyright();
    void testGetContextBetween();
    void testPrepareContext();

private:
    Context::DocumentContextReader createReader(const QString &text);
    QSharedPointer<Settings::CodeCompletionSettings> createSettingsForWholeFile();
    QSharedPointer<Settings::CodeCompletionSettings> createSettingsForLines(
        int linesBefore, int linesAfter);
};

} // namespace QodeAssist
