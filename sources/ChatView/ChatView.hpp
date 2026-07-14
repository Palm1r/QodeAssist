// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <functional>

#include <utils/id.h>

#include <QQuickView>
#include <QShortcut>

namespace QodeAssist::Skills {
class SkillsManager;
}

namespace QodeAssist::Chat {

class SessionFileRegistry;

class ChatView : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(bool isPin READ isPin WRITE setIsPin NOTIFY isPinChanged FINAL)
public:
    ChatView(
        QQmlEngine *engine,
        SessionFileRegistry *sessionFileRegistry,
        Skills::SkillsManager *skillsManager);

    bool isPin() const;
    void setIsPin(bool newIsPin);

signals:
    void isPinChanged();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void saveSettings();
    void restoreSettings();
    void bindCommandShortcut(Utils::Id commandId, const std::function<void()> &onActivated);

    bool m_isPin;
};

} // namespace QodeAssist::Chat
