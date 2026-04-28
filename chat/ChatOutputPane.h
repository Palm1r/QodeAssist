// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ChatView/ChatWidget.hpp"
#include <coreplugin/ioutputpane.h>

namespace QodeAssist::Chat {

class ChatOutputPane : public Core::IOutputPane
{
    Q_OBJECT

public:
    explicit ChatOutputPane(QQmlEngine* engine, QObject *parent = nullptr);
    ~ChatOutputPane() override;

    QWidget *outputWidget(QWidget *parent) override;
    QList<QWidget *> toolBarWidgets() const override;
    void clearContents() override;
    void visibilityChanged(bool visible) override;
    void setFocus() override;
    bool hasFocus() const override;
    bool canFocus() const override;
    bool canNavigate() const override;
    bool canNext() const override;
    bool canPrevious() const override;
    void goToNext() override;
    void goToPrev() override;

private:
    ChatWidget *m_chatWidget;
};

} // namespace QodeAssist::Chat
