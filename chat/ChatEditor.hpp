// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <coreplugin/editormanager/ieditor.h>

class QQmlEngine;

namespace QodeAssist::Skills {
class SkillsManager;
}

namespace QodeAssist::Chat {

class ChatDocument;
class ChatWidget;
class SessionFileRegistry;

class ChatEditor : public Core::IEditor
{
    Q_OBJECT

public:
    ChatEditor(
        QQmlEngine *engine,
        SessionFileRegistry *sessionFileRegistry,
        Skills::SkillsManager *skillsManager);
    ~ChatEditor() override;

    Core::IDocument *document() const override;
    QWidget *toolBar() override;
    Core::IEditor *duplicate() override;

    void consumePendingChatFile();

private:
    QQmlEngine *m_engine;
    SessionFileRegistry *m_sessionFileRegistry;
    Skills::SkillsManager *m_skillsManager;
    ChatDocument *m_document;
    ChatWidget *m_chatWidget;
};

} // namespace QodeAssist::Chat
