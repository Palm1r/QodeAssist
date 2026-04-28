// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

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

    bool m_isPin;
    QShortcut *m_closeShortcut;
};

} // namespace QodeAssist::Chat
