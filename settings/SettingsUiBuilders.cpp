// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsUiBuilders.hpp"

#include "SettingsTheme.hpp"

#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QWidget>

namespace QodeAssist::Settings {

void applyMutedSmallCaps(QLabel *label)
{
    QFont f = label->font();
    f.setPixelSize(10);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.4);
    label->setFont(f);
    QPalette p = label->palette();
    p.setColor(QPalette::WindowText, p.color(QPalette::Mid));
    label->setPalette(p);
}

QLabel *makeSectionHeader(const QString &title, QWidget *parent)
{
    auto *header = new QLabel(title.toUpper(), parent);
    applyMutedSmallCaps(header);
    header->setContentsMargins(8, 4, 8, 4);
    header->setAutoFillBackground(true);
    const Theme theme = themeFor(parent ? parent->palette() : QPalette());
    header->setStyleSheet(
        QStringLiteral("QLabel { background:%1; border-top:1px solid %2;"
                       " border-bottom:1px solid %2; }")
            .arg(theme.listHeaderBg, theme.rowSeparator));
    return header;
}

QLabel *makeHintLabel(const QString &text, QWidget *parent)
{
    auto *h = new QLabel(text, parent);
    QFont hf = h->font();
    hf.setPixelSize(11);
    h->setFont(hf);
    h->setWordWrap(true);
    QPalette p = h->palette();
    p.setColor(QPalette::WindowText, p.color(QPalette::Mid));
    h->setPalette(p);
    return h;
}

QHBoxLayout *singleField(QWidget *w)
{
    auto *lay = new QHBoxLayout;
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    lay->addWidget(w, 1);
    return lay;
}

namespace {

QLabel *makeFormLabel(const QString &text)
{
    auto *l = new QLabel(text);
    l->setMinimumWidth(96);
    l->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    return l;
}

} // namespace

FormBuilder::FormBuilder(QGridLayout *grid, int startRow)
    : m_grid(grid)
    , m_row(startRow)
{}

FormBuilder &FormBuilder::row(const QString &label, QLayout *value, const QString &hint)
{
    m_grid->addWidget(makeFormLabel(label), m_row, 0, Qt::AlignTop);
    auto *holder = new QWidget;
    holder->setLayout(value);
    m_grid->addWidget(holder, m_row, 1);
    ++m_row;
    if (!hint.isEmpty()) {
        m_grid->addWidget(makeHintLabel(hint), m_row, 1);
        ++m_row;
    }
    return *this;
}

FormBuilder &FormBuilder::row(const QString &label, QWidget *value, const QString &hint)
{
    return row(label, singleField(value), hint);
}

} // namespace QodeAssist::Settings
