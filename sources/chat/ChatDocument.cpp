// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatDocument.hpp"

#include <utils/result.h>

#include "plugin/QodeAssistConstants.hpp"
#include "plugin/QodeAssisttr.h"

namespace QodeAssist::Chat {

ChatDocument::ChatDocument(QObject *parent)
    : Core::IDocument(parent)
{
    setId(Constants::QODE_ASSIST_CHAT_EDITOR_ID);
    setMimeType("text/plain");
    setTemporary(true);
    setPreferredDisplayName(Tr::tr("QodeAssist Chat"));
}

QByteArray ChatDocument::contents() const
{
    return {};
}

Utils::Result<> ChatDocument::setContents(const QByteArray &)
{
    return Utils::ResultOk;
}

bool ChatDocument::isModified() const
{
    return false;
}

bool ChatDocument::isSaveAsAllowed() const
{
    return false;
}

bool ChatDocument::shouldAutoSave() const
{
    return false;
}

} // namespace QodeAssist::Chat
