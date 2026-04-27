// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ContentFile.hpp"
#include <QList>
#include <QString>

namespace QodeAssist::Context {

class TokenUtils
{
public:
    static int estimateTokens(const QString &text);
    static int estimateFileTokens(const Context::ContentFile &file);
    static int estimateFilesTokens(const QList<Context::ContentFile> &files);
};

} // namespace QodeAssist::Context
