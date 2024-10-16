/*
 * Copyright (C) 2024 Petr Mironychev
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

#include "ChatRootView.hpp"
#include <QtGui/qclipboard.h>
#include <utils/theme/theme.h>
#include <utils/utilsicons.h>

#include "GeneralSettings.hpp"

namespace QodeAssist::Chat {

ChatRootView::ChatRootView(QQuickItem *parent)
    : QQuickItem(parent)
    , m_chatModel(new ChatModel(this))
    , m_clientInterface(new ClientInterface(m_chatModel, this))
{
    auto &settings = Settings::generalSettings();

    connect(&settings.chatModelName,
            &Utils::BaseAspect::changed,
            this,
            &ChatRootView::currentTemplateChanged);
    generateColors();
}

ChatModel *ChatRootView::chatModel() const
{
    return m_chatModel;
}

QColor ChatRootView::backgroundColor() const
{
    return Utils::creatorColor(Utils::Theme::BackgroundColorNormal);
}

void ChatRootView::sendMessage(const QString &message) const
{
    m_clientInterface->sendMessage(message);
}

void ChatRootView::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

void ChatRootView::cancelRequest()
{
    m_clientInterface->cancelRequest();
}

void ChatRootView::generateColors()
{
    QColor baseColor = backgroundColor();
    bool isDarkTheme = baseColor.lightness() < 128;

    if (isDarkTheme) {
        m_primaryColor = generateColor(baseColor, 0.1, 1.2, 1.4);
        m_secondaryColor = generateColor(baseColor, -0.1, 1.1, 1.2);
        m_codeColor = generateColor(baseColor, 0.05, 0.8, 1.1);
    } else {
        m_primaryColor = generateColor(baseColor, 0.05, 1.05, 1.1);
        m_secondaryColor = generateColor(baseColor, -0.05, 1.1, 1.2);
        m_codeColor = generateColor(baseColor, 0.02, 0.95, 1.05);
    }
}

QColor ChatRootView::generateColor(const QColor &baseColor,
                                   float hueShift,
                                   float saturationMod,
                                   float lightnessMod)
{
    float h, s, l, a;
    baseColor.getHslF(&h, &s, &l, &a);
    bool isDarkTheme = l < 0.5;

    h = fmod(h + hueShift + 1.0, 1.0);

    s = qBound(0.0f, s * saturationMod, 1.0f);

    if (isDarkTheme) {
        l = qBound(0.0f, l * lightnessMod, 1.0f);
    } else {
        l = qBound(0.0f, l / lightnessMod, 1.0f);
    }

    h = qBound(0.0f, h, 1.0f);
    s = qBound(0.0f, s, 1.0f);
    l = qBound(0.0f, l, 1.0f);
    a = qBound(0.0f, a, 1.0f);

    return QColor::fromHslF(h, s, l, a);
}

QString ChatRootView::currentTemplate() const
{
    auto &settings = Settings::generalSettings();
    return settings.chatModelName();
}

QColor ChatRootView::primaryColor() const
{
    return m_primaryColor;
}

QColor ChatRootView::secondaryColor() const
{
    return m_secondaryColor;
}

QColor ChatRootView::codeColor() const
{
    return m_codeColor;
}

} // namespace QodeAssist::Chat
