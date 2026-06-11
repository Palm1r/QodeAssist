// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentSlotWidget.hpp"

#include "SettingsTr.hpp"

#include <QEvent>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPalette>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QVBoxLayout>

namespace QodeAssist::Settings {

namespace {

bool isDarkPalette(const QPalette &p)
{
    return p.color(QPalette::Window).lightness() < 128;
}

QFont monoFont(int pixelSize)
{
    QFont f;
    f.setFamilies({QStringLiteral("SF Mono"),
                   QStringLiteral("Cascadia Code"),
                   QStringLiteral("Consolas"),
                   QStringLiteral("Liberation Mono"),
                   QStringLiteral("Menlo"),
                   QStringLiteral("Courier New")});
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(pixelSize);
    return f;
}

struct Tone
{
    QString cardBg;
    QString cardBd;
    QString textSoft;
    QString textMute;
    QString textFaint;
};

Tone toneFor(bool dark)
{
    return dark ? Tone{"#333333", "#4a4a4a", "#c2c2c2", "#9a9a9a", "#7a7a7a"}
                : Tone{"#f6f6f6", "#bdbdbd", "#3a3a3a", "#5a5a5a", "#8a8a8a"};
}

struct PillTone
{
    QString bg;
    QString fg;
    QString bd;
};

PillTone pillTone(Pill::Kind kind, bool dark)
{
    switch (kind) {
    case Pill::Template:
        return dark ? PillTone{"#2c3f5a", "#cfe1f7", "#4a6286"}
                    : PillTone{"#dbe7f6", "#1f3f73", "#a8c1e0"};
    case Pill::On:
        return dark ? PillTone{"#2f4530", "#bce0bd", "#4a6c4b"}
                    : PillTone{"#dbe9d3", "#2c5a1c", "#a3bc97"};
    case Pill::Off:
        return dark ? PillTone{"#3a3a3a", "#8a8a8a", "#4a4a4a"}
                    : PillTone{"#ececec", "#7a7a7a", "#c8c8c8"};
    case Pill::User:
        return dark ? PillTone{"#4a3f24", "#e6cd92", "#6a5a30"}
                    : PillTone{"#f0e4cf", "#75541a", "#cdb98a"};
    case Pill::Tag:
        return dark ? PillTone{"#2e2e3a", "#b9b9cf", "#46465a"}
                    : PillTone{"#e7e7f2", "#46466e", "#c1c1d5"};
    }
    return {};
}

} // namespace

// -- Pill --------------------------------------------------------------

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

void Pill::setKind(Kind kind)
{
    if (m_kind == kind)
        return;
    m_kind = kind;
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

    const auto t = pillTone(m_kind, isDarkPalette(palette()));
    setStyleSheet(QStringLiteral(
                      "QLabel { background-color: %1; color: %2;"
                      " border: 1px solid %3; padding: 1px 7px; }")
                      .arg(t.bg, t.fg, t.bd));
}

// -- AgentSlotWidget ---------------------------------------------------

AgentSlotWidget::AgentSlotWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("AgentSlot"));

    m_name = new QLabel(this);
    QFont nameFont = m_name->font();
    nameFont.setBold(true);
    m_name->setFont(nameFont);
    m_name->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_sourcePill = new Pill(Pill::User, {}, this);
    m_sourcePill->hide();

    m_changeBtn = new QPushButton(Tr::tr("Change…"), this);
    m_changeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_changeBtn, &QPushButton::clicked, this, &AgentSlotWidget::changeRequested);

    m_editBtn = new QPushButton(Tr::tr("Edit"), this);
    m_editBtn->setCursor(Qt::PointingHandCursor);
    m_editBtn->setToolTip(Tr::tr("Open this agent in the Agents settings page."));
    connect(m_editBtn, &QPushButton::clicked, this, &AgentSlotWidget::editRequested);

    auto makeFieldLabel = [this]() {
        auto *l = new QLabel(this);
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return l;
    };
    auto makeMonoValue = [this]() {
        auto *l = new QLabel(this);
        l->setFont(monoFont(11));
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        l->setMinimumWidth(0);
        l->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        return l;
    };
    auto makePlainValue = [this]() {
        auto *l = new QLabel(this);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        l->setMinimumWidth(0);
        l->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        return l;
    };

    m_modelLabel = makeFieldLabel();
    m_modelLabel->setText(Tr::tr("Model"));
    m_modelValue = makeMonoValue();

    m_urlLabel = makeFieldLabel();
    m_urlLabel->setText(Tr::tr("Provider"));
    m_urlValue = makeMonoValue();

    m_endpointLabel = makeFieldLabel();
    m_endpointLabel->setText(Tr::tr("Endpoint"));
    m_endpointValue = makeMonoValue();

    m_templateLabel = makeFieldLabel();
    m_templateLabel->setText(Tr::tr("Template"));
    m_templateValue = makePlainValue();

    m_description = new QLabel(this);
    m_description->setWordWrap(true);
    m_description->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont descFont = m_description->font();
    descFont.setItalic(true);
    m_description->setFont(descFont);

    m_thinkingPill = new Pill(Pill::Off, {}, this);
    m_toolsPill = new Pill(Pill::Off, {}, this);

    // Header row
    auto *nameRow = new QHBoxLayout;
    nameRow->setContentsMargins(0, 0, 0, 0);
    nameRow->setSpacing(6);
    nameRow->addWidget(m_name);
    nameRow->addWidget(m_sourcePill);
    nameRow->addStretch(1);

    auto *buttonsBox = new QHBoxLayout;
    buttonsBox->setContentsMargins(0, 0, 0, 0);
    buttonsBox->setSpacing(4);
    buttonsBox->addWidget(m_editBtn);
    buttonsBox->addWidget(m_changeBtn);

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(8);
    headerRow->addLayout(nameRow, 1);
    headerRow->addLayout(buttonsBox, 0);

    // Two-column field grid
    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(2);
    grid->setColumnMinimumWidth(0, 62);
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 1);

    grid->addWidget(m_modelLabel, 0, 0);
    grid->addWidget(m_modelValue, 0, 1);
    grid->addWidget(m_urlLabel, 1, 0);
    grid->addWidget(m_urlValue, 1, 1);
    grid->addWidget(m_endpointLabel, 2, 0);
    grid->addWidget(m_endpointValue, 2, 1);
    grid->addWidget(m_templateLabel, 3, 0);
    grid->addWidget(m_templateValue, 3, 1);

    auto *pillsRow = new QHBoxLayout;
    pillsRow->setContentsMargins(0, 0, 0, 0);
    pillsRow->setSpacing(4);
    pillsRow->addWidget(m_thinkingPill);
    pillsRow->addWidget(m_toolsPill);
    pillsRow->addStretch(1);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(6);
    outer->addLayout(headerRow);
    outer->addLayout(grid);
    outer->addWidget(m_description);
    outer->addLayout(pillsRow);

    applyTheme();
    clear();
}

void AgentSlotWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (m_inApplyTheme)
        return;
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        applyTheme();
}

void AgentSlotWidget::applyTheme()
{
    if (m_inApplyTheme)
        return;
    QScopedValueRollback<bool> guard(m_inApplyTheme, true);

    const Tone t = toneFor(isDarkPalette(palette()));

    setStyleSheet(QStringLiteral(
                      "#AgentSlot { background-color: %1; border: 1px solid %2; }")
                      .arg(t.cardBg, t.cardBd));

    auto applyColor = [](QLabel *label, const QString &color) {
        QPalette p = label->palette();
        p.setColor(QPalette::WindowText, QColor(color));
        label->setPalette(p);
    };

    for (QLabel *l : {m_modelLabel, m_urlLabel, m_endpointLabel, m_templateLabel})
        applyColor(l, t.textMute);

    applyColor(m_description, t.textSoft);
}

void AgentSlotWidget::setAgentConfig(const AgentConfig &cfg)
{
    m_name->setText(cfg.name);

    if (cfg.isUserSource()) {
        m_sourcePill->setText(Tr::tr("User"));
        m_sourcePill->show();
    } else {
        m_sourcePill->hide();
    }

    m_modelValue->setText(cfg.model);
    m_modelValue->setToolTip(cfg.model);
    // The agent profile no longer carries a URL — the URL belongs to
    // the referenced provider instance and is editable on the Providers
    // settings page. Surface the instance name in this row instead.
    m_urlValue->setText(cfg.providerInstance);
    m_urlValue->setToolTip(cfg.providerInstance);
    m_endpointValue->setText(cfg.endpoint);
    m_endpointValue->setToolTip(cfg.endpoint);
    // Templates are now inline in the agent profile — no separate name to
    // surface. Keep the row but show '—' so the layout stays stable.
    m_templateLabel->hide();
    m_templateValue->hide();

    m_description->setText(cfg.description.isEmpty()
                               ? Tr::tr("No description provided.")
                               : cfg.description);

    const Tone t = toneFor(isDarkPalette(palette()));
    QPalette descPal = m_description->palette();
    descPal.setColor(QPalette::WindowText,
                     QColor(cfg.description.isEmpty() ? t.textFaint : t.textSoft));
    m_description->setPalette(descPal);

    m_thinkingPill->setKind(cfg.enableThinking ? Pill::On : Pill::Off);
    m_thinkingPill->setText(cfg.enableThinking ? Tr::tr("thinking on")
                                               : Tr::tr("thinking off"));
    m_toolsPill->setKind(cfg.enableTools ? Pill::On : Pill::Off);
    m_toolsPill->setText(cfg.enableTools ? Tr::tr("tools on")
                                         : Tr::tr("tools off"));
    m_thinkingPill->show();
    m_toolsPill->show();

    m_editBtn->setEnabled(!cfg.name.isEmpty());
}

void AgentSlotWidget::clear()
{
    const Tone t = toneFor(isDarkPalette(palette()));

    m_name->setText(QStringLiteral("<i style=\"color:%1\">%2</i>")
                        .arg(t.textFaint, Tr::tr("(no agent selected)")));
    m_sourcePill->hide();

    for (QLabel *l : {m_modelValue, m_urlValue, m_endpointValue, m_templateValue}) {
        l->clear();
        l->setToolTip({});
    }

    m_description->setText({});
    m_thinkingPill->hide();
    m_toolsPill->hide();
    m_editBtn->setEnabled(false);
}

} // namespace QodeAssist::Settings
