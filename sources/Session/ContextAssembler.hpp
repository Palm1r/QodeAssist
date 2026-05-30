// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <ContextData.hpp>

#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <vector>

namespace QodeAssist {

class Message;

namespace ContextAssembler {

using ContentLoader = std::function<QString(const QString &storedPath)>;

struct PinnedBlock
{
    QString id;
    QString text;
};

struct Manifest
{
    qsizetype systemChars = 0;
    int historyMessages = 0;
    int wireMessages = 0;
    qsizetype textChars = 0;
    qsizetype thinkingChars = 0;
    qsizetype toolChars = 0;
    qsizetype pinnedChars = 0;
    int imageBlocks = 0;
    int toolUseBlocks = 0;
    int toolResultBlocks = 0;
    int pinnedBlocks = 0;
    int unsupportedBlocks = 0;
    bool hasCompletionContext = false;
    QStringList elided;

    QString summary() const;
};

Templates::ContextData assemble(
    const std::vector<Message> &history,
    const QString &systemPrompt,
    const ContentLoader &loader,
    const QVector<PinnedBlock> &pinned = {},
    Manifest *outManifest = nullptr);

} // namespace ContextAssembler
} // namespace QodeAssist
