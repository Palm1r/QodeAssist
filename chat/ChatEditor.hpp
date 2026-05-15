// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <coreplugin/editormanager/ieditor.h>

class QQmlEngine;

namespace QodeAssist::Chat {

class ChatDocument;
class ChatWidget;
class SessionFileRegistry;

// Editor-area host for the chat. Each editor (including a split duplicate) owns its own
// ChatWidget and therefore its own independent chat session.
class ChatEditor : public Core::IEditor
{
    Q_OBJECT

public:
    ChatEditor(QQmlEngine *engine, SessionFileRegistry *sessionFileRegistry);
    ~ChatEditor() override;

    Core::IDocument *document() const override;
    QWidget *toolBar() override;
    Core::IEditor *duplicate() override;

    void consumePendingChatFile();

private:
    QQmlEngine *m_engine;
    SessionFileRegistry *m_sessionFileRegistry;
    ChatDocument *m_document;
    ChatWidget *m_chatWidget;
};

} // namespace QodeAssist::Chat
