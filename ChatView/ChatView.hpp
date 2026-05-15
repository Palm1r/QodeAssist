// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>

#include <utils/id.h>

#include <QQuickView>
#include <QShortcut>

namespace QodeAssist::Chat {

class ChatView : public QQuickView
{
    Q_OBJECT
    Q_PROPERTY(bool isPin READ isPin WRITE setIsPin NOTIFY isPinChanged FINAL)
public:
    ChatView(QQmlEngine* engine);

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
