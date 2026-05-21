// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFont>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QString>
#include <QWidget>

namespace QodeAssist::Settings {

inline QFont monospaceFont(int pixelSize = 11)
{
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setStyleHint(QFont::Monospace);
    if (pixelSize > 0)
        f.setPixelSize(pixelSize);
    return f;
}

inline bool isDarkPalette(const QPalette &p)
{
    return p.color(QPalette::Window).lightness() < 128;
}

inline int addFormRow(
    QGridLayout *grid, int row, const QString &label, QLayout *value, const QString &hint = {})
{
    auto *l = new QLabel(label);
    l->setMinimumWidth(96);
    l->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(l, row, 0, Qt::AlignTop);
    auto *holder = new QWidget;
    holder->setLayout(value);
    grid->addWidget(holder, row, 1);
    if (hint.isEmpty())
        return row + 1;
    auto *h = new QLabel(hint);
    QFont hf = h->font();
    hf.setPixelSize(11);
    h->setFont(hf);
    h->setWordWrap(true);
    QPalette p = h->palette();
    p.setColor(QPalette::WindowText, p.color(QPalette::Mid));
    h->setPalette(p);
    grid->addWidget(h, row + 1, 1);
    return row + 2;
}

inline QHBoxLayout *singleField(QWidget *w)
{
    auto *lay = new QHBoxLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    lay->addWidget(w, 1);
    return lay;
}

} // namespace QodeAssist::Settings
