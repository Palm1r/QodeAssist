// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "Pill.hpp"

#include "SettingsTheme.hpp"

#include <utils/theme/theme.h>

#include <QEvent>
#include <QFont>
#include <QScopedValueRollback>

namespace QodeAssist::Settings {

namespace {

struct PillTone
{
    QColor bg;
    QColor fg;
    QColor border;
};

PillTone toneFor(Pill::Kind kind)
{
    const QColor cardBg = Utils::creatorColor(Utils::Theme::BackgroundColorNormal);
    const QColor text = Utils::creatorColor(Utils::Theme::TextColorNormal);
    const QColor textMuted = Utils::creatorColor(Utils::Theme::PanelTextColorMid);
    const QColor textFaint = Utils::creatorColor(Utils::Theme::TextColorDisabled);
    const QColor accent = Utils::creatorColor(Utils::Theme::TextColorLink);
    const QColor activeBg = Utils::creatorColor(Utils::Theme::BackgroundColorSelected);
    const QColor border = Utils::creatorColor(Utils::Theme::SplitterColor);
    const bool dark = cardBg.lightness() < 128;

    const auto tint = [&](const QColor &role) -> PillTone {
        return {mix(cardBg, role, 0.16), dark ? mix(text, role, 0.65) : role, mix(cardBg, role, 0.42)};
    };

    switch (kind) {
    case Pill::On:
        return tint(Utils::creatorColor(Utils::Theme::IconsRunColor));
    case Pill::Off:
        return {mix(cardBg, textFaint, 0.12), textFaint, mix(cardBg, textFaint, 0.30)};
    case Pill::User:
        return tint(Utils::creatorColor(Utils::Theme::IconsWarningColor));
    case Pill::Accent:
    case Pill::Match:
        return tint(accent);
    case Pill::Active:
        return {activeBg, text, accent};
    case Pill::Tag:
    case Pill::Neutral:
        return {mix(cardBg, textMuted, 0.14), textMuted, mix(cardBg, textMuted, 0.32)};
    }
    return {cardBg, text, border};
}

} // namespace

Pill::Pill(Kind kind, const QString &text, QWidget *parent)
    : QLabel(text, parent)
    , m_kind(kind)
{
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    setAlignment(Qt::AlignCenter);
    QFont f = font();
    f.setPixelSize(11);
    setFont(f);
    applyTheme();
}

void Pill::changeEvent(QEvent *event)
{
    QLabel::changeEvent(event);
    if (m_inApplyTheme)
        return;
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        applyTheme();
}

void Pill::applyTheme()
{
    if (m_inApplyTheme)
        return;
    QScopedValueRollback<bool> guard(m_inApplyTheme, true);

    const PillTone t = toneFor(m_kind);
    setStyleSheet(QStringLiteral("QLabel { background-color:%1; color:%2;"
                                 " border:1px solid %3; border-radius:3px;"
                                 " padding:1px 7px; font-size:11px; }")
                      .arg(cssColor(t.bg), cssColor(t.fg), cssColor(t.border)));
}

} // namespace QodeAssist::Settings
