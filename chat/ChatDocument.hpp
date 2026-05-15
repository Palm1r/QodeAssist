// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <coreplugin/idocument.h>

namespace QodeAssist::Chat {

// Backing document for a chat editor view. The chat persists itself through its own
// autosave history file, so this document is purely a placeholder for the editor area
// and never participates in Qt Creator's save infrastructure.
class ChatDocument : public Core::IDocument
{
    Q_OBJECT

public:
    explicit ChatDocument(QObject *parent = nullptr);

    QByteArray contents() const override;
    Utils::Result<> setContents(const QByteArray &contents) override;
    bool isModified() const override;
    bool isSaveAsAllowed() const override;
    bool shouldAutoSave() const override;
};

} // namespace QodeAssist::Chat
