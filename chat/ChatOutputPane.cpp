/* 
 * Copyright (C) 2024-2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#include "ChatOutputPane.h"

#include "QodeAssisttr.h"

namespace QodeAssist::Chat {

ChatOutputPane::ChatOutputPane(QObject *parent)
    : Core::IOutputPane(parent)
    , m_chatWidget(new ChatWidget)
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
