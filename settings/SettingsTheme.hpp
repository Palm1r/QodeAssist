// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QString>

namespace QodeAssist::Settings {

inline bool isDarkPalette(const QPalette &p)
{
    return p.color(QPalette::Window).lightness() < 128;
}

inline QColor mix(const QColor &a, const QColor &b, double t)
{
    const double s = 1.0 - t;
    return QColor::fromRgbF(
        a.redF() * s + b.redF() * t,
        a.greenF() * s + b.greenF() * t,
        a.blueF() * s + b.blueF() * t,
        1.0);
}

inline QString cssColor(const QColor &c)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(c.alphaF(), 0, 'f', 3);
}

inline QFont monospaceFont(int pixelSize = 11)
{
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setStyleHint(QFont::Monospace);
    if (pixelSize > 0)
        f.setPixelSize(pixelSize);
    return f;
}

} // namespace QodeAssist::Settings
