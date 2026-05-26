// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QString>

namespace QodeAssist::Settings {

struct Theme
{
    bool dark = false;
    QString listHeaderBg;
    QString rowSeparator;
    QString rowSelectedBg;
    QString codeBg;
};

inline bool isDarkPalette(const QPalette &p)
{
    return p.color(QPalette::Window).lightness() < 128;
}

inline Theme themeFor(const QPalette &p)
{
    const bool dark = isDarkPalette(p);
    if (dark)
        return {true,
                QStringLiteral("#262626"),
                QStringLiteral("#3a3a3a"),
                QStringLiteral("#2c4060"),
                QStringLiteral("#1f1f1f")};
    return {false,
            QStringLiteral("#f0f0f0"),
            QStringLiteral("#dcdcdc"),
            QStringLiteral("#cfe2ff"),
            QStringLiteral("#f4f4f4")};
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
