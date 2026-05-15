// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <coreplugin/editormanager/ieditorfactory.h>

class QQmlEngine;

namespace QodeAssist::Chat {

class SessionFileRegistry;

class ChatEditorFactory : public Core::IEditorFactory
{
public:
    ChatEditorFactory(QQmlEngine *engine, SessionFileRegistry *sessionFileRegistry);
};

} // namespace QodeAssist::Chat
