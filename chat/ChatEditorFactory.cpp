// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatEditorFactory.hpp"

#include "ChatEditor.hpp"
#include "QodeAssistConstants.hpp"
#include "QodeAssisttr.h"

namespace QodeAssist::Chat {

ChatEditorFactory::ChatEditorFactory(
    QQmlEngine *engine,
    SessionFileRegistry *sessionFileRegistry,
    Skills::SkillsManager *skillsManager)
{
    setId(Constants::QODE_ASSIST_CHAT_EDITOR_ID);
    setDisplayName(Tr::tr("QodeAssist Chat"));
    setEditorCreator([engine, sessionFileRegistry, skillsManager] {
        return new ChatEditor(engine, sessionFileRegistry, skillsManager);
    });
}

} // namespace QodeAssist::Chat
