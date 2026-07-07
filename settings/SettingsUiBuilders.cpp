// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "SettingsUiBuilders.hpp"

#include "SettingsTheme.hpp"

#include <utils/theme/theme.h>

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
    p.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorMid));
    label->setPalette(p);
}

QString filterHighlightedHtml(const QString &text, const QString &lowerFilter)
{
    if (lowerFilter.isEmpty())
        return text.toHtmlEscaped();
    QColor mark = Utils::creatorColor(Utils::Theme::TextColorLink);
    mark.setAlphaF(0.30);
    const QString markCss
        = QStringLiteral("background-color:%1;border-radius:2px;").arg(cssColor(mark));
    const QString lowerText = text.toLower();
    QString out;
    int pos = 0;
    while (true) {
        const int hit = lowerText.indexOf(lowerFilter, pos);
        if (hit < 0) {
            out += text.mid(pos).toHtmlEscaped();
            break;
        }
        out += text.mid(pos, hit - pos).toHtmlEscaped();
        out += QStringLiteral("<span style=\"%1\">%2</span>")
                   .arg(markCss, text.mid(hit, lowerFilter.size()).toHtmlEscaped());
        pos = hit + int(lowerFilter.size());
    }
    return out;
}

void styleSourceBadge(QLabel *label, bool user)
{
    QFont f = label->font();
    f.setBold(true);
    f.setPixelSize(10);
    label->setFont(f);
    const QColor accent = Utils::creatorColor(
        user ? Utils::Theme::TextColorLink : Utils::Theme::PanelTextColorMid);
    QColor bg = accent;
    bg.setAlphaF(0.12);
    QColor border = accent;
    border.setAlphaF(0.45);
    label->setStyleSheet(QStringLiteral(
                             "QLabel { color:%1; background:%2; border:1px solid %3;"
                             " border-radius:8px; padding:1px 8px; }")
                             .arg(cssColor(accent), cssColor(bg), cssColor(border)));
}

QLabel *makeSectionHeader(const QString &title, QWidget *parent)
{
    auto *header = new QLabel(title.toUpper(), parent);
    QFont f = header->font();
    f.setPixelSize(11);
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
    header->setFont(f);
    QPalette p = header->palette();
    p.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorLight));
    header->setPalette(p);
    header->setContentsMargins(8, 5, 8, 5);
    header->setAutoFillBackground(true);
    const QColor base = Utils::creatorColor(Utils::Theme::BackgroundColorNormal);
    const QColor mid = Utils::creatorColor(Utils::Theme::PanelTextColorMid);
    header->setStyleSheet(QStringLiteral(
                              "QLabel { background:%1; border-top:1px solid %2;"
                              " border-bottom:1px solid %2; }")
                              .arg(cssColor(mix(base, mid, 0.16)), cssColor(mix(base, mid, 0.30))));
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
    p.setColor(QPalette::WindowText, Utils::creatorColor(Utils::Theme::PanelTextColorMid));
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
