// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ChatOutputPane.h"

#include "QodeAssisttr.h"

namespace QodeAssist::Chat {

ChatOutputPane::ChatOutputPane(QQmlEngine* engine, QObject *parent)
    : Core::IOutputPane(parent)
    , m_chatWidget{new ChatWidget{engine}}
{
    setId("QodeAssistChat");
    setDisplayName(Tr::tr("QodeAssist Chat"));
    setPriorityInStatusBar(-40);
}

ChatOutputPane::~ChatOutputPane()
{
    delete m_chatWidget;
}

QWidget *ChatOutputPane::outputWidget(QWidget *)
{
    return m_chatWidget;
}

QList<QWidget *> ChatOutputPane::toolBarWidgets() const
{
    return {};
}

void ChatOutputPane::clearContents()
{
    m_chatWidget->clear();
}

void ChatOutputPane::visibilityChanged(bool visible)
{
    if (visible)
        m_chatWidget->scrollToBottom();
}

void ChatOutputPane::setFocus()
{
    m_chatWidget->setFocus();
}

bool ChatOutputPane::hasFocus() const
{
    return m_chatWidget->hasFocus();
}

bool ChatOutputPane::canFocus() const
{
    return true;
}

bool ChatOutputPane::canNavigate() const
{
    return false;
}

bool ChatOutputPane::canNext() const
{
    return false;
}

bool ChatOutputPane::canPrevious() const
{
    return false;
}

void ChatOutputPane::goToNext() {}

void ChatOutputPane::goToPrev() {}

} // namespace QodeAssist::Chat
