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
#include <utils/theme/theme.h>

#include <Agent.hpp>
#include <AgentConfig.hpp>
#include <AgentFactory.hpp>
#include <AgentRouter.hpp>

#include "AgentSelectionDialog.hpp"
#include "SettingsTheme.hpp"
#include "SettingsTr.hpp"

namespace QodeAssist::Settings {

namespace {

enum class PillKind {
    Template,
    On,
    Off,
    User,
    Active,
    Match,
    Tag,
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
    const QColor bg = Utils::creatorColor(Utils::Theme::BackgroundColorNormal);
    const QColor fg = Utils::creatorColor(Utils::Theme::TextColorNormal);
    const QColor muted = Utils::creatorColor(Utils::Theme::PanelTextColorMid);
    const QColor faint = Utils::creatorColor(Utils::Theme::TextColorDisabled);
    const QColor link = Utils::creatorColor(Utils::Theme::TextColorLink);
    const QColor selBg = Utils::creatorColor(Utils::Theme::BackgroundColorSelected);
    const QColor line = Utils::creatorColor(Utils::Theme::SplitterColor);
    const bool isDark = bg.lightness() < 128;

    const auto tint = [&](const QColor &role) -> Pill {
        return {mix(bg, role, 0.16), isDark ? mix(fg, role, 0.65) : role, mix(bg, role, 0.42)};
    };

    switch (k) {
    case PillKind::Template:
        return tint(link);
    case PillKind::On:
        return tint(Utils::creatorColor(Utils::Theme::IconsRunColor));
    case PillKind::Off:
        return {mix(bg, faint, 0.12), faint, mix(bg, faint, 0.30)};
    case PillKind::User:
        return tint(Utils::creatorColor(Utils::Theme::IconsWarningColor));
    case PillKind::Active:
        return {selBg, fg, link};
    case PillKind::Match:
        return tint(link);
    case PillKind::Tag:
    case PillKind::Neutral:
        return {mix(bg, muted, 0.14), muted, mix(bg, muted, 0.32)};
    }
    return {bg, fg, line};
}

Theme themeFor(const QWidget *)
{
    const QColor bg = Utils::creatorColor(Utils::Theme::BackgroundColorNormal);
    const QColor fg = Utils::creatorColor(Utils::Theme::TextColorNormal);
    const QColor muted = Utils::creatorColor(Utils::Theme::PanelTextColorMid);
    const QColor faint = Utils::creatorColor(Utils::Theme::TextColorDisabled);
    const QColor line = Utils::creatorColor(Utils::Theme::SplitterColor);
    const QColor selBg = Utils::creatorColor(Utils::Theme::BackgroundColorSelected);
    const bool isDark = bg.lightness() < 128;

    Theme t;
    t.dark = isDark;
    t.pageBg = mix(bg, isDark ? QColor(Qt::black) : QColor(Qt::white), 0.04);
    t.cardBg = bg;
    t.cardBorder = line;
    t.groupBorder = line;
    t.rowSeparator = line;
    t.rowMatchBg = selBg;
    t.listHeader = mix(bg, fg, 0.05);
    t.text = fg;
    t.textSoft = muted;
    t.textMute = muted;
    t.textFaint = faint;
    t.matchChipBg = mix(bg, fg, 0.06);
    t.matchChipBorder = line;
    t.matchChipText = muted;
    t.codeBg = mix(bg, fg, 0.06);
    t.codeBorder = line;
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
               "padding:1px 6px; border-radius:3px;"
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
    AgentRosterRow(
        int index,
        const QString &name,
        const AgentConfig *cfg,
        const QString &model,
        bool active,
        bool first,
        bool last,
        bool orderable,
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
    QWidget *buildMetaLine(const AgentConfig *cfg, bool active, bool showMatch, const Theme &t);
    QWidget *buildActions(const Theme &t, bool first, bool last);

    int m_index;
};

AgentRosterRow::AgentRosterRow(
    int index,
    const QString &name,
    const AgentConfig *cfg,
    const QString &model,
    bool active,
    bool first,
    bool last,
    bool orderable,
    const Theme &theme,
    QWidget *parent)
    : QFrame(parent)
    , m_index(index)
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

    if (orderable) {
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

        connect(upBtn, &QToolButton::clicked, this, [this]() { emit moveUpRequested(m_index); });
        connect(dnBtn, &QToolButton::clicked, this, [this]() { emit moveDownRequested(m_index); });

        auto *idxLbl = new QLabel(QStringLiteral("%1.").arg(index + 1), this);
        QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        idxLbl->setFont(monoFont);
        QPalette idxPal = idxLbl->palette();
        idxPal.setColor(QPalette::WindowText, active ? theme.text : theme.textFaint);
        idxLbl->setPalette(idxPal);
        idxLbl->setFixedWidth(22);
        idxLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        outer->addWidget(idxLbl);
    }

    auto *body = new QVBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(2);

    const QString displayName = cfg ? cfg->name : tr("%1 (missing)").arg(name);
    const bool isUser = cfg && cfg->isUserSource();

    body->addWidget(buildIdentityLine(displayName, model, active, isUser, theme));
    body->addWidget(buildMetaLine(cfg, active, /*showMatch*/ orderable, theme));
    outer->addLayout(body, /*stretch*/ 1);

    outer->addWidget(buildActions(theme, first, last));

    if (cfg && !cfg->description.isEmpty())
        setToolTip(cfg->description);
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

QWidget *AgentRosterRow::buildMetaLine(
    const AgentConfig *cfg, bool active, bool showMatch, const Theme &t)
{
    auto *w = new QWidget(this);
    auto *line = new QHBoxLayout(w);
    line->setContentsMargins(0, 0, 0, 0);
    line->setSpacing(4);

    if (cfg) {
        if (showMatch) {
            const auto sm = summarise(cfg->match);
            auto *chip = new QLabel(w);
            const QColor bg = active ? t.pill(PillKind::Match).bg : t.matchChipBg;
            const QColor bd = active ? t.pill(PillKind::Match).border : t.matchChipBorder;
            const QColor fg = active ? t.pill(PillKind::Match).fg : t.matchChipText;
            QString chipText = QStringLiteral(
                                   "<span style='opacity:0.7'>%1</span> "
                                   "<span style='color:%2'>%3:</span> %4")
                                   .arg(
                                       sm.icon,
                                       hex(active ? t.pill(PillKind::Match).fg : t.textFaint),
                                       sm.kind,
                                       sm.value.toHtmlEscaped());
            chip->setTextFormat(Qt::RichText);
            chip->setText(chipText);
            chip->setStyleSheet(QStringLiteral(
                                    "background:%1; color:%2; border:1px solid %3;"
                                    "padding:1px 6px; border-radius:3px; font-size:11px;")
                                    .arg(hex(bg), hex(fg), hex(bd)));
            chip->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
            line->addWidget(chip);

            auto *arrow = new QLabel(QStringLiteral("→"), w);
            QPalette ap = arrow->palette();
            ap.setColor(QPalette::WindowText, t.textFaint);
            arrow->setPalette(ap);
            line->addWidget(arrow);
        }

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
    m_titleLabel = new QLabel(this);
    QFont tf = m_titleLabel->font();
    tf.setBold(true);
    m_titleLabel->setFont(tf);
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
        QStringLiteral(
            "QFrame#rosterCard { background:%1; border:1px solid %2; border-radius:4px; }")
            .arg(hex(t.cardBg), hex(t.cardBorder)));
    m_rowsLayout = new QVBoxLayout(m_rowsFrame);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(0);

    m_emptyHint = new QLabel(tr("No agent selected yet — use \"Add agent…\" below."), m_rowsFrame);
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

void AgentRosterWidget::setSlot(
    const QString &title, const QString &hint, const QStringList &presetTags)
{
    m_presetTags = presetTags;
    m_titleLabel->setText(title);
    m_hintLabel->setText(hint);
    m_hintLabel->setVisible(!hint.isEmpty());
}

void AgentRosterWidget::setRoster(const QStringList &names, AgentFactory *factory)
{
    if (m_factory != factory) {
        QObject::disconnect(m_factoryConn);
        QObject::disconnect(m_modelConn);
        m_factory = factory;
        if (m_factory) {
            m_factoryConn = connect(m_factory, &AgentFactory::agentsChanged, this, [this] {
                rebuildRows();
            });
            m_modelConn = connect(
                m_factory, &AgentFactory::agentModelChanged, this, [this](const QString &name) {
                    if (m_names.contains(name))
                        rebuildRows();
                });
        }
    }
    m_names = names;
    rebuildRows();
}

void AgentRosterWidget::setOrderable(bool orderable)
{
    if (m_orderable == orderable)
        return;
    m_orderable = orderable;
    rebuildRows();
}

void AgentRosterWidget::setSingle(bool single)
{
    if (m_single == single)
        return;
    m_single = single;
    if (single)
        m_orderable = false;
    rebuildRows();
}

void AgentRosterWidget::applyMode()
{
    const bool hasEntry = !m_names.isEmpty();
    m_addBtn->setText((m_single && hasEntry) ? tr("Change agent…") : tr("+ Add agent…"));

    QString footer;
    if (!m_single)
        footer = m_orderable ? tr("first matching agent is used")
                             : tr("you pick the active agent in the chat panel");
    m_footerHint->setText(footer);
    m_footerHint->setVisible(!footer.isEmpty());
}

void AgentRosterWidget::setRoutingContext(const AgentRouter::Context &ctx)
{
    m_routingCtx = ctx;
    if (!m_names.isEmpty())
        rebuildRows();
}

void AgentRosterWidget::recomputeActive()
{
    if (!m_orderable || !m_factory || m_names.isEmpty()
        || (m_routingCtx.filePath.isEmpty() && m_routingCtx.projectName.isEmpty())) {
        m_activeIndex = -1;
        return;
    }
    const QString picked = AgentRouter::pickAgent(m_names, m_routingCtx, *m_factory);
    m_activeIndex = picked.isEmpty() ? -1 : m_names.indexOf(picked);
}

void AgentRosterWidget::rebuildRows()
{
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

    applyMode();

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
        const QString model = cfg ? cfg->model : QString();
        auto *row = new AgentRosterRow(
            i,
            name,
            cfg,
            model,
            i == m_activeIndex,
            /*first*/ i == 0,
            /*last*/ i == m_names.size() - 1,
            m_orderable,
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

    AgentSelectionDialog dialog(
        m_factory->configs(),
        /*currentName*/ m_single ? m_names.value(0) : QString{},
        m_factory.data(),
        m_presetTags,
        Core::ICore::dialogParent());
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString picked = dialog.selectedName();
    if (picked.isEmpty())
        return;

    if (m_single) {
        if (m_names.size() == 1 && m_names.first() == picked)
            return;
        m_names = {picked};
    } else {
        if (m_names.contains(picked))
            return;
        m_names.append(picked);
    }
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
