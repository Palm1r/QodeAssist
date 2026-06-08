// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentRosterWidget.hpp"

#include <QApplication>
#include <QColor>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <coreplugin/icore.h>

#include <Agent.hpp>
#include <AgentConfig.hpp>
#include <AgentFactory.hpp>
#include <AgentRouter.hpp>

#include "AgentSelectionDialog.hpp"
#include "SettingsTr.hpp"

namespace QodeAssist::Settings {

namespace {

enum class PillKind {
    Template,
    On,       // capability on (thinking/tools)
    Off,      // capability off ("plain")
    User,     // user-defined agent
    Active,   // ✓ active
    Match,    // matched-this-row chip background
    Tag,      // free-form discoverability tag from AgentConfig::tags
    Neutral,
};

struct Theme
{
    bool dark = false;

    QColor pageBg;
    QColor cardBg;
    QColor cardBorder;
    QColor groupBorder;
    QColor rowSeparator;
    QColor rowMatchBg;
    QColor listHeader;

    QColor text;
    QColor textSoft;
    QColor textMute;
    QColor textFaint;

    QColor matchChipBg;
    QColor matchChipBorder;
    QColor matchChipText;

    QColor codeBg;
    QColor codeBorder;

    struct Pill
    {
        QColor bg, fg, border;
    };
    Pill pill(PillKind k) const;
};

Theme::Pill Theme::pill(PillKind k) const
{
    if (dark) {
        switch (k) {
        case PillKind::Template: return {{0x2c, 0x3f, 0x5a}, {0xcf, 0xe1, 0xf7}, {0x4a, 0x62, 0x86}};
        case PillKind::On:       return {{0x2f, 0x45, 0x30}, {0xbc, 0xe0, 0xbd}, {0x4a, 0x6c, 0x4b}};
        case PillKind::Off:      return {{0x3a, 0x3a, 0x3a}, {0x8a, 0x8a, 0x8a}, {0x4a, 0x4a, 0x4a}};
        case PillKind::User:     return {{0x4a, 0x3f, 0x24}, {0xe6, 0xcd, 0x92}, {0x6a, 0x5a, 0x30}};
        case PillKind::Active:   return {{0x3a, 0x6a, 0x28}, {0xff, 0xff, 0xff}, {0x4a, 0x80, 0x38}};
        case PillKind::Match:    return {{0x27, 0x38, 0x4a}, {0xbd, 0xd7, 0xee}, {0x3f, 0x5a, 0x78}};
        case PillKind::Tag:      return {{0x2e, 0x2e, 0x3a}, {0xb9, 0xb9, 0xcf}, {0x46, 0x46, 0x5a}};
        case PillKind::Neutral:  return {{0x3a, 0x3a, 0x3a}, {0xcf, 0xcf, 0xcf}, {0x4a, 0x4a, 0x4a}};
        }
    } else {
        switch (k) {
        case PillKind::Template: return {{0xdb, 0xe7, 0xf6}, {0x1f, 0x3f, 0x73}, {0xa8, 0xc1, 0xe0}};
        case PillKind::On:       return {{0xdb, 0xe9, 0xd3}, {0x2c, 0x5a, 0x1c}, {0xa3, 0xbc, 0x97}};
        case PillKind::Off:      return {{0xec, 0xec, 0xec}, {0x7a, 0x7a, 0x7a}, {0xc8, 0xc8, 0xc8}};
        case PillKind::User:     return {{0xf0, 0xe4, 0xcf}, {0x75, 0x54, 0x1a}, {0xcd, 0xb9, 0x8a}};
        case PillKind::Active:   return {{0x2c, 0x5a, 0x1c}, {0xff, 0xff, 0xff}, {0x2c, 0x5a, 0x1c}};
        case PillKind::Match:    return {{0xd6, 0xe8, 0xf7}, {0x1a, 0x4a, 0x7a}, {0x8a, 0xb1, 0xd5}};
        case PillKind::Tag:      return {{0xe7, 0xe7, 0xf2}, {0x46, 0x46, 0x6e}, {0xc1, 0xc1, 0xd5}};
        case PillKind::Neutral:  return {{0xe3, 0xe3, 0xe3}, {0x3a, 0x3a, 0x3a}, {0xbc, 0xbc, 0xbc}};
        }
    }
    return {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
}

Theme themeFor(const QWidget *w)
{
    const QPalette pal = w ? w->palette() : QApplication::palette();
    const bool dark = pal.color(QPalette::Window).lightness() < 128;
    Theme t;
    t.dark = dark;
    if (dark) {
        t.pageBg = {0x2b, 0x2b, 0x2b};
        t.cardBg = {0x2f, 0x2f, 0x2f};
        t.cardBorder = {0x4a, 0x4a, 0x4a};
        t.groupBorder = {0x4a, 0x4a, 0x4a};
        t.rowSeparator = {0x3a, 0x3a, 0x3a};
        t.rowMatchBg = {0x2d, 0x3d, 0x24};
        t.listHeader = {0x25, 0x25, 0x25};
        t.text = {0xe6, 0xe6, 0xe6};
        t.textSoft = {0xc2, 0xc2, 0xc2};
        t.textMute = {0x9a, 0x9a, 0x9a};
        t.textFaint = {0x7a, 0x7a, 0x7a};
        t.matchChipBg = {0x26, 0x26, 0x26};
        t.matchChipBorder = {0x3a, 0x3a, 0x3a};
        t.matchChipText = {0xc2, 0xc2, 0xc2};
        t.codeBg = {0x26, 0x26, 0x26};
        t.codeBorder = {0x3a, 0x3a, 0x3a};
    } else {
        t.pageBg = {0xed, 0xed, 0xed};
        t.cardBg = {0xf8, 0xf8, 0xf8};
        t.cardBorder = {0xbd, 0xbd, 0xbd};
        t.groupBorder = {0xb8, 0xb8, 0xb8};
        t.rowSeparator = {0xe0, 0xe0, 0xe0};
        t.rowMatchBg = {0xe8, 0xf3, 0xdf};
        t.listHeader = {0xe8, 0xe8, 0xe8};
        t.text = {0x1c, 0x1c, 0x1c};
        t.textSoft = {0x3a, 0x3a, 0x3a};
        t.textMute = {0x5a, 0x5a, 0x5a};
        t.textFaint = {0x8a, 0x8a, 0x8a};
        t.matchChipBg = {0xea, 0xea, 0xea};
        t.matchChipBorder = {0xcf, 0xcf, 0xcf};
        t.matchChipText = {0x3a, 0x3a, 0x3a};
        t.codeBg = {0xea, 0xea, 0xea};
        t.codeBorder = {0xcf, 0xcf, 0xcf};
    }
    return t;
}

QString hex(const QColor &c)
{
    return c.name(QColor::HexRgb);
}

QString pillStyle(const Theme &t, PillKind k)
{
    const auto p = t.pill(k);
    return QStringLiteral(
               "background:%1; color:%2; border:1px solid %3;"
               "padding:0px 5px; border-radius:2px;"
               "font-size:10px;")
        .arg(hex(p.bg), hex(p.fg), hex(p.border));
}

QLabel *makePill(const QString &text, PillKind kind, const Theme &t, QWidget *parent = nullptr)
{
    auto *l = new QLabel(text, parent);
    l->setStyleSheet(pillStyle(t, kind));
    l->setAlignment(Qt::AlignCenter);
    return l;
}

struct MatchSummary
{
    QString icon;
    QString kind;
    QString value;
};

MatchSummary summarise(const AgentConfig::Match &m)
{
    if (m.isEmpty())
        return {QStringLiteral("·"), AgentRosterWidget::tr("fallback"),
                AgentRosterWidget::tr("any file")};
    if (!m.filePatterns.isEmpty())
        return {QStringLiteral("*"), AgentRosterWidget::tr("path"),
                m.filePatterns.join(QStringLiteral(", "))};
    if (!m.pathPatterns.isEmpty())
        return {QStringLiteral("*"), AgentRosterWidget::tr("path"),
                m.pathPatterns.join(QStringLiteral(", "))};
    if (!m.projectNames.isEmpty())
        return {QStringLiteral("▣"), AgentRosterWidget::tr("project"),
                m.projectNames.join(QStringLiteral(", "))};
    return {QStringLiteral("·"), AgentRosterWidget::tr("any"), {}};
}

} // namespace

class AgentRosterRow : public QFrame
{
    Q_OBJECT
public:
    AgentRosterRow(int index,
                   const QString &name,
                   const AgentConfig *cfg,
                   bool active,
                   bool first,
                   bool last,
                   const Theme &theme,
                   QWidget *parent = nullptr);

signals:
    void moveUpRequested(int index);
    void moveDownRequested(int index);
    void editRequested(int index);
    void removeRequested(int index);

private:
    QWidget *buildIdentityLine(const QString &displayName,
                               const QString &model,
                               bool active,
                               bool isUser,
                               const Theme &t);
    QWidget *buildMetaLine(const AgentConfig *cfg, bool active, const Theme &t);
    QWidget *buildActions(const Theme &t, bool first, bool last);

    int m_index;
};

AgentRosterRow::AgentRosterRow(int index,
                               const QString &name,
                               const AgentConfig *cfg,
                               bool active,
                               bool first,
                               bool last,
                               const Theme &theme,
                               QWidget *parent)
    : QFrame(parent), m_index(index)
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, active ? theme.rowMatchBg : theme.cardBg);
    setPalette(pal);

    setStyleSheet(QStringLiteral("AgentRosterRow { border:0; border-top:%1; }")
                      .arg(index == 0 ? QStringLiteral("0px")
                                      : QStringLiteral("1px solid %1").arg(hex(theme.rowSeparator))));

    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(8, 6, 8, 6);
    outer->setSpacing(8);

    auto *moveCol = new QVBoxLayout;
    moveCol->setContentsMargins(0, 0, 0, 0);
    moveCol->setSpacing(1);
    auto makeArrow = [&](const QString &glyph, const QString &tip, bool enabled) {
        auto *b = new QToolButton(this);
        b->setText(glyph);
        b->setToolTip(tip);
        b->setEnabled(enabled);
        b->setAutoRaise(true);
        b->setFixedSize(18, 14);
        QFont f = b->font();
        f.setPointSizeF(f.pointSizeF() * 0.75);
        b->setFont(f);
        return b;
    };
    auto *upBtn = makeArrow(QStringLiteral("▲"), tr("Move up"), !first);
    auto *dnBtn = makeArrow(QStringLiteral("▼"), tr("Move down"), !last);
    moveCol->addWidget(upBtn);
    moveCol->addWidget(dnBtn);
    outer->addLayout(moveCol);

    auto *idxLbl = new QLabel(QStringLiteral("%1.").arg(index + 1), this);
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    idxLbl->setFont(monoFont);
    QPalette idxPal = idxLbl->palette();
    idxPal.setColor(QPalette::WindowText, active ? theme.text : theme.textFaint);
    idxLbl->setPalette(idxPal);
    idxLbl->setFixedWidth(22);
    idxLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    outer->addWidget(idxLbl);

    auto *body = new QVBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(2);

    const QString displayName = cfg ? cfg->name : tr("%1 (missing)").arg(name);
    const QString model = cfg ? cfg->model : QString();
    const bool isUser = cfg && cfg->isUserSource();

    body->addWidget(buildIdentityLine(displayName, model, active, isUser, theme));
    body->addWidget(buildMetaLine(cfg, active, theme));
    outer->addLayout(body, /*stretch*/ 1);

    outer->addWidget(buildActions(theme, first, last));

    if (cfg && !cfg->description.isEmpty())
        setToolTip(cfg->description);

    connect(upBtn, &QToolButton::clicked, this, [this]() { emit moveUpRequested(m_index); });
    connect(dnBtn, &QToolButton::clicked, this, [this]() { emit moveDownRequested(m_index); });
}

QWidget *AgentRosterRow::buildIdentityLine(const QString &displayName,
                                           const QString &model,
                                           bool active,
                                           bool isUser,
                                           const Theme &t)
{
    auto *w = new QWidget(this);
    auto *line = new QHBoxLayout(w);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(6);

    auto *nameLbl = new QLabel(displayName, w);
    QFont nameFont = nameLbl->font();
    nameFont.setBold(true);
    nameLbl->setFont(nameFont);
    line->addWidget(nameLbl);

    if (!model.isEmpty()) {
        auto *modelLbl = new QLabel(model, w);
        modelLbl->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        QPalette mp = modelLbl->palette();
        mp.setColor(QPalette::WindowText, t.textSoft);
        modelLbl->setPalette(mp);
        line->addWidget(modelLbl);
    }

    if (active)
        line->addWidget(makePill(QStringLiteral("✓ ") + tr("active"), PillKind::Active, t, w));
    if (isUser)
        line->addWidget(makePill(tr("user"), PillKind::User, t, w));

    line->addStretch(1);
    return w;
}

QWidget *AgentRosterRow::buildMetaLine(const AgentConfig *cfg, bool active, const Theme &t)
{
    auto *w = new QWidget(this);
    auto *line = new QHBoxLayout(w);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(4);

    if (cfg) {
        const auto sm = summarise(cfg->match);
        auto *chip = new QLabel(w);
        const QColor bg = active ? t.pill(PillKind::Match).bg : t.matchChipBg;
        const QColor bd = active ? t.pill(PillKind::Match).border : t.matchChipBorder;
        const QColor fg = active ? t.pill(PillKind::Match).fg : t.matchChipText;
        QString chipText = QStringLiteral(
                               "<span style='opacity:0.7'>%1</span> "
                               "<span style='color:%2'>%3:</span> %4")
                               .arg(sm.icon,
                                    hex(active ? t.pill(PillKind::Match).fg : t.textFaint),
                                    sm.kind,
                                    sm.value.toHtmlEscaped());
        chip->setTextFormat(Qt::RichText);
        chip->setText(chipText);
        chip->setStyleSheet(QStringLiteral("background:%1; color:%2; border:1px solid %3;"
                                           "padding:0px 6px; border-radius:2px; font-size:11px;")
                                .arg(hex(bg), hex(fg), hex(bd)));
        chip->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        line->addWidget(chip);

        auto *arrow = new QLabel(QStringLiteral("→"), w);
        QPalette ap = arrow->palette();
        ap.setColor(QPalette::WindowText, t.textFaint);
        arrow->setPalette(ap);
        line->addWidget(arrow);

        if (!cfg->providerInstance.isEmpty())
            line->addWidget(makePill(cfg->providerInstance, PillKind::Template, t, w));

        constexpr int kMaxTagPills = 3;
        const int tagCount = cfg->tags.size();
        for (int i = 0; i < std::min(tagCount, kMaxTagPills); ++i)
            line->addWidget(makePill(cfg->tags.at(i), PillKind::Tag, t, w));
        if (tagCount > kMaxTagPills) {
            auto *more = makePill(QStringLiteral("+%1").arg(tagCount - kMaxTagPills),
                                  PillKind::Tag, t, w);
            more->setToolTip(cfg->tags.mid(kMaxTagPills).join(QStringLiteral(", ")));
            line->addWidget(more);
        }
    } else {
        auto *missing = new QLabel(tr("agent configuration is no longer available"), w);
        QPalette mp = missing->palette();
        mp.setColor(QPalette::WindowText, t.textMute);
        missing->setPalette(mp);
        line->addWidget(missing);
    }
    line->addStretch(1);
    return w;
}

QWidget *AgentRosterRow::buildActions(const Theme &, bool /*first*/, bool /*last*/)
{
    auto *w = new QWidget(this);
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(4);

    auto *edit = new QToolButton(w);
    edit->setText(tr("Edit…"));
    edit->setToolButtonStyle(Qt::ToolButtonTextOnly);
    edit->setAutoRaise(false);
    edit->setFocusPolicy(Qt::TabFocus);
    edit->setMinimumHeight(22);

    auto *remove = new QToolButton(w);
    remove->setText(QStringLiteral("✕"));
    remove->setToolTip(tr("Remove from list"));
    remove->setAutoRaise(false);
    remove->setFixedSize(22, 22);

    l->addWidget(edit);
    l->addWidget(remove);

    connect(edit, &QToolButton::clicked, this, [this]() { emit editRequested(m_index); });
    connect(remove, &QToolButton::clicked, this, [this]() { emit removeRequested(m_index); });

    return w;
}

AgentRosterWidget::AgentRosterWidget(QWidget *parent)
    : QWidget(parent)
{
    const Theme t = themeFor(this);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(6);

    auto *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(6);
    m_accentDot = new QLabel(this);
    m_accentDot->setFixedSize(10, 10);
    m_titleLabel = new QLabel(this);
    QFont tf = m_titleLabel->font();
    tf.setBold(true);
    m_titleLabel->setFont(tf);
    titleRow->addWidget(m_accentDot);
    titleRow->addWidget(m_titleLabel);
    titleRow->addStretch(1);
    outer->addLayout(titleRow);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    QPalette hp = m_hintLabel->palette();
    hp.setColor(QPalette::WindowText, t.textMute);
    m_hintLabel->setPalette(hp);
    QFont hf = m_hintLabel->font();
    hf.setPointSizeF(hf.pointSizeF() * 0.9);
    m_hintLabel->setFont(hf);
    outer->addWidget(m_hintLabel);

    m_rowsFrame = new QFrame(this);
    m_rowsFrame->setObjectName(QStringLiteral("rosterCard"));
    m_rowsFrame->setStyleSheet(
        QStringLiteral("QFrame#rosterCard { background:%1; border:1px solid %2; border-radius:2px; }")
            .arg(hex(t.cardBg), hex(t.cardBorder)));
    m_rowsLayout = new QVBoxLayout(m_rowsFrame);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(0);

    m_emptyHint = new QLabel(tr("No agents in roster. Click \"Add agent…\" to populate."),
                             m_rowsFrame);
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setContentsMargins(10, 12, 10, 12);
    QFont eh = m_emptyHint->font();
    eh.setItalic(true);
    m_emptyHint->setFont(eh);
    QPalette ep = m_emptyHint->palette();
    ep.setColor(QPalette::WindowText, t.textFaint);
    m_emptyHint->setPalette(ep);
    m_rowsLayout->addWidget(m_emptyHint);

    outer->addWidget(m_rowsFrame);

    auto *footer = new QHBoxLayout;
    footer->setContentsMargins(0, 0, 0, 0);
    footer->setSpacing(6);
    m_addBtn = new QPushButton(tr("+ Add agent…"), this);
    footer->addWidget(m_addBtn);
    footer->addStretch(1);
    m_footerHint = new QLabel(tr("first matching agent is used"), this);
    QPalette fp = m_footerHint->palette();
    fp.setColor(QPalette::WindowText, t.textMute);
    m_footerHint->setPalette(fp);
    QFont ff = m_footerHint->font();
    ff.setPointSizeF(ff.pointSizeF() * 0.9);
    m_footerHint->setFont(ff);
    footer->addWidget(m_footerHint);
    outer->addLayout(footer);

    connect(m_addBtn, &QPushButton::clicked, this, &AgentRosterWidget::onAddClicked);
}

void AgentRosterWidget::setSlot(const QString &title, const QString &hint, const QColor &accent)
{
    m_titleLabel->setText(title);
    m_hintLabel->setText(hint);
    m_hintLabel->setVisible(!hint.isEmpty());
    if (accent.isValid()) {
        m_accentDot->setStyleSheet(
            QStringLiteral("background:%1; border:1px solid %2;")
                .arg(accent.name(), accent.darker(140).name()));
        m_accentDot->setVisible(true);
    } else {
        m_accentDot->setStyleSheet({});
        m_accentDot->setVisible(false);
    }
}

void AgentRosterWidget::setRoster(const QStringList &names, AgentFactory *factory)
{
    m_factory = factory;
    m_names = names;
    rebuildRows();
}

void AgentRosterWidget::setRoutingContext(const AgentRouter::Context &ctx)
{
    m_routingCtx = ctx;
    recomputeActive();
    rebuildRows();
}

void AgentRosterWidget::recomputeActive()
{
    if (!m_factory || m_names.isEmpty()
        || (m_routingCtx.filePath.isEmpty() && m_routingCtx.projectName.isEmpty())) {
        m_activeIndex = -1;
        return;
    }
    const QString picked = AgentRouter::pickAgent(m_names, m_routingCtx, *m_factory);
    m_activeIndex = picked.isEmpty() ? -1 : m_names.indexOf(picked);
}

void AgentRosterWidget::rebuildRows()
{
    // Tear down existing row widgets (keep the empty hint).
    while (m_rowsLayout->count() > 0) {
        QLayoutItem *it = m_rowsLayout->takeAt(0);
        if (auto *w = it->widget()) {
            if (w == m_emptyHint)
                w->setVisible(false);
            else
                w->deleteLater();
        }
        delete it;
    }

    if (m_names.isEmpty()) {
        m_emptyHint->setVisible(true);
        m_rowsLayout->addWidget(m_emptyHint);
        return;
    }

    recomputeActive();
    const Theme t = themeFor(this);

    for (int i = 0; i < m_names.size(); ++i) {
        const QString &name = m_names.at(i);
        const AgentConfig *cfg = m_factory ? m_factory->configByName(name) : nullptr;
        auto *row = new AgentRosterRow(i,
                                       name,
                                       cfg,
                                       i == m_activeIndex,
                                       /*first*/ i == 0,
                                       /*last*/ i == m_names.size() - 1,
                                       t,
                                       m_rowsFrame);
        connect(row, &AgentRosterRow::moveUpRequested, this, &AgentRosterWidget::onRowMoveUp);
        connect(row, &AgentRosterRow::moveDownRequested, this, &AgentRosterWidget::onRowMoveDown);
        connect(row, &AgentRosterRow::editRequested, this, &AgentRosterWidget::onRowEdit);
        connect(row, &AgentRosterRow::removeRequested, this, &AgentRosterWidget::onRowRemove);
        m_rowsLayout->addWidget(row);
    }
}

void AgentRosterWidget::onAddClicked()
{
    if (!m_factory)
        return;

    AgentSelectionDialog dialog(m_factory->configs(),
                                /*currentName*/ QString{},
                                m_factory.data(),
                                Core::ICore::dialogParent());
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString picked = dialog.selectedName();
    if (picked.isEmpty() || m_names.contains(picked))
        return;

    m_names.append(picked);
    rebuildRows();
    emit rosterChanged(m_names);
}

void AgentRosterWidget::onRowMoveUp(int index)
{
    if (index <= 0 || index >= m_names.size())
        return;
    m_names.swapItemsAt(index, index - 1);
    rebuildRows();
    emit rosterChanged(m_names);
}

void AgentRosterWidget::onRowMoveDown(int index)
{
    if (index < 0 || index >= m_names.size() - 1)
        return;
    m_names.swapItemsAt(index, index + 1);
    rebuildRows();
    emit rosterChanged(m_names);
}

void AgentRosterWidget::onRowRemove(int index)
{
    if (index < 0 || index >= m_names.size())
        return;
    m_names.removeAt(index);
    rebuildRows();
    emit rosterChanged(m_names);
}

void AgentRosterWidget::onRowEdit(int index)
{
    if (index < 0 || index >= m_names.size())
        return;
    emit editAgentRequested(m_names.at(index));
}

} // namespace QodeAssist::Settings

#include "AgentRosterWidget.moc"
