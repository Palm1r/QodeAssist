// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentSelectionDialog.hpp"

#include "Pill.hpp"
#include "PipelinesConfig.hpp"
#include "SettingsTr.hpp"
#include "SettingsUiBuilders.hpp"
#include "TagFilterStrip.hpp"

#include <coreplugin/icore.h>

#include <AgentFactory.hpp>

#include <algorithm>
#include <QDialogButtonBox>
#include <QEnterEvent>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMap>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSet>
#include <QTimer>
#include <QVBoxLayout>

namespace QodeAssist::Settings {

ListRowCard::ListRowCard(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("ListRowCard"));
    setAttribute(Qt::WA_StyledBackground, true);
    setCursor(Qt::PointingHandCursor);
    setFrameShape(QFrame::NoFrame);
    applyTheme();
}

bool ListRowCard::matches(const QString &needle) const
{
    if (needle.isEmpty())
        return true;
    return m_searchHaystack.contains(needle.toLower());
}

void ListRowCard::setSelected(bool selected)
{
    if (m_selected == selected)
        return;
    m_selected = selected;
    applyTheme();
}

bool ListRowCard::hasAllTags(const QSet<QString> &activeTags) const
{
    for (const QString &tag : activeTags)
        if (!m_itemTags.contains(tag))
            return false;
    return true;
}

void ListRowCard::setFilterHighlight(const QString &lowerNeedle)
{
    Q_UNUSED(lowerNeedle)
}

void ListRowCard::buildSearchHaystack(const QStringList &parts)
{
    m_searchHaystack = parts.join(QLatin1Char(' ')).toLower();
}

void ListRowCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked();
    QFrame::mousePressEvent(event);
}

void ListRowCard::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit activated();
    QFrame::mouseDoubleClickEvent(event);
}

void ListRowCard::enterEvent(QEnterEvent *event)
{
    QFrame::enterEvent(event);
    m_hover = true;
    applyTheme();
}

void ListRowCard::leaveEvent(QEvent *event)
{
    QFrame::leaveEvent(event);
    m_hover = false;
    applyTheme();
}

void ListRowCard::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);
    if (m_inApplyTheme)
        return;
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        applyTheme();
}

void ListRowCard::applyTheme()
{
    if (m_inApplyTheme)
        return;
    QScopedValueRollback<bool> guard(m_inApplyTheme, true);

    const auto t = CardStyle::toneFor(CardStyle::isDark(palette()));
    QString bg = t.bg;
    QString bd = t.cardBd;
    if (m_selected) {
        bg = t.selectedBg;
        bd = t.selectedBd;
    } else if (m_hover) {
        bg = t.hoverBg;
    }
    setStyleSheet(
        QStringLiteral("#ListRowCard { background-color: %1; border: 1px solid %2; }").arg(bg, bd));
}

AgentRowCard::AgentRowCard(const AgentConfig &cfg, QWidget *parent)
    : ListRowCard(parent)
{
    setItemName(cfg.name);
    setItemTags(cfg.tags);
    QStringList haystack{
        cfg.name, cfg.providerInstance, cfg.model, cfg.description, cfg.systemPrompt, cfg.endpoint};
    haystack += cfg.tags;
    buildSearchHaystack(haystack);

    auto *name = new QLabel(cfg.name.toHtmlEscaped(), this);
    QFont nameFont = name->font();
    nameFont.setBold(true);
    nameFont.setPixelSize(13);
    name->setFont(nameFont);
    name->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    name->setTextFormat(Qt::RichText);
    m_nameLabel = name;

    QLabel *model = nullptr;
    if (!cfg.model.isEmpty()) {
        model = new QLabel(cfg.model.toHtmlEscaped(), this);
        model->setFont(CardStyle::monoFont(11));
        model->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        model->setMinimumWidth(0);
        model->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        model->setTextFormat(Qt::RichText);
        m_modelLabel = model;
        m_model = cfg.model;
    }

    Pill *sourcePill = nullptr;
    if (cfg.isUserSource()) {
        sourcePill = new Pill(Pill::User, Tr::tr("User"), this);
    }

    auto *headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(6);
    headerRow->addWidget(name);
    if (sourcePill)
        headerRow->addWidget(sourcePill);
    if (model)
        headerRow->addWidget(model, 1);
    else
        headerRow->addStretch(1);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 5, 8, 5);
    outer->setSpacing(2);
    outer->addLayout(headerRow);

    if (!cfg.tags.isEmpty()) {
        constexpr int kMaxTagPills = 4;
        auto *tagsRow = new QHBoxLayout;
        tagsRow->setContentsMargins(0, 0, 0, 0);
        tagsRow->setSpacing(4);
        const int tagCount = cfg.tags.size();
        for (int i = 0; i < std::min(tagCount, kMaxTagPills); ++i)
            tagsRow->addWidget(new Pill(Pill::Tag, cfg.tags.at(i), this));
        if (tagCount > kMaxTagPills) {
            auto *more
                = new Pill(Pill::Tag, QStringLiteral("+%1").arg(tagCount - kMaxTagPills), this);
            more->setToolTip(cfg.tags.mid(kMaxTagPills).join(QStringLiteral(", ")));
            tagsRow->addWidget(more);
        }
        tagsRow->addStretch(1);
        outer->addLayout(tagsRow);
    }

    if (model) {
        const auto t = CardStyle::toneFor(CardStyle::isDark(palette()));
        QPalette mp = model->palette();
        mp.setColor(QPalette::WindowText, QColor(t.textMute));
        model->setPalette(mp);
    }

    QStringList capabilityParts;
    if (!cfg.endpoint.isEmpty())
        capabilityParts << cfg.endpoint;
    capabilityParts << (cfg.enableThinking ? Tr::tr("thinking") : Tr::tr("no-thinking"));
    capabilityParts << (cfg.enableTools ? Tr::tr("tools") : Tr::tr("no-tools"));

    QString tooltip;
    if (!cfg.description.isEmpty())
        tooltip += cfg.description + QStringLiteral("\n\n");
    if (!cfg.model.isEmpty())
        tooltip += Tr::tr("Model: %1\n").arg(cfg.model);
    if (!cfg.providerInstance.isEmpty())
        tooltip += Tr::tr("Provider instance: %1\n").arg(cfg.providerInstance);
    if (!cfg.systemPrompt.isEmpty())
        tooltip += Tr::tr("System prompt: %1\n").arg(cfg.systemPrompt);
    tooltip += capabilityParts.join(QStringLiteral(" · "));
    if (!cfg.tags.isEmpty())
        tooltip += QStringLiteral("\n")
                   + Tr::tr("Tags: %1").arg(cfg.tags.join(QStringLiteral(", ")));
    setToolTip(tooltip.trimmed());
}

void AgentRowCard::setFilterHighlight(const QString &lowerNeedle)
{
    if (m_lowerNeedle == lowerNeedle)
        return;
    m_lowerNeedle = lowerNeedle;
    m_nameLabel->setText(filterHighlightedHtml(itemName(), lowerNeedle));
    if (m_modelLabel)
        m_modelLabel->setText(filterHighlightedHtml(m_model, lowerNeedle));
}

ProviderSection::ProviderSection(const QString &name, QWidget *parent)
    : QWidget(parent)
{
    m_arrow = new QLabel(QStringLiteral("▾"));
    m_label = new QLabel(name);
    CardStyle::applySectionFont(m_label);

    QFont arrowFont = m_label->font();
    arrowFont.setCapitalization(QFont::MixedCase);
    m_arrow->setFont(arrowFont);
    QPalette ap = m_arrow->palette();
    ap.setColor(QPalette::WindowText, ap.color(QPalette::Mid));
    m_arrow->setPalette(ap);

    m_count = new QLabel;
    CardStyle::applySectionFont(m_count);

    m_header = new QFrame;
    m_header->setObjectName(QStringLiteral("ProviderHeader"));
    m_header->setCursor(Qt::PointingHandCursor);
    m_header->setFrameShape(QFrame::NoFrame);
    auto *headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(2, 4, 2, 2);
    headerLayout->setSpacing(6);
    headerLayout->addWidget(m_arrow);
    headerLayout->addWidget(m_label);
    headerLayout->addStretch(1);
    headerLayout->addWidget(m_count);
    m_header->installEventFilter(this);

    m_content = new QWidget;
    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(4);
    m_content->setVisible(false);
    m_arrow->setText(QStringLiteral("▸"));
    m_expanded = false;

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(m_header);
    outer->addWidget(m_content);
}

void ProviderSection::addCard(ListRowCard *card)
{
    m_contentLayout->addWidget(card);
    m_cards.append(card);
}

int ProviderSection::applyFilter(const QString &needle, const QSet<QString> &activeTags)
{
    const QString lowerNeedle = needle.toLower();
    int visible = 0;
    for (auto *card : m_cards) {
        const bool show = card->matches(needle) && card->hasAllTags(activeTags);
        card->setVisible(show);
        card->setFilterHighlight(lowerNeedle);
        if (show)
            ++visible;
    }
    if (m_count)
        m_count->setText(QString::number(visible));
    return visible;
}

void ProviderSection::setExpanded(bool expanded)
{
    if (m_expanded == expanded)
        return;
    m_expanded = expanded;
    m_content->setVisible(expanded);
    m_arrow->setText(expanded ? QStringLiteral("▾") : QStringLiteral("▸"));
}

bool ProviderSection::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_header && event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            setExpanded(!m_expanded);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

AgentSelectionDialog::AgentSelectionDialog(
    const std::vector<AgentConfig> &configs,
    const QString &currentName,
    AgentFactory *agentFactory,
    const QStringList &presetTags,
    QWidget *parent)
    : QDialog(parent)
    , m_agentFactory(agentFactory)
    , m_presetTags(presetTags)
{
    const bool isChange = !currentName.isEmpty();
    setWindowTitle(isChange ? Tr::tr("Change Agent") : Tr::tr("Add Agent"));
    resize(720, 600);
    setMinimumSize(560, 420);
    setSizeGripEnabled(true);

    if (!m_agentFactory)
        m_localConfigs = configs;

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(Tr::tr("Filter by name, provider, model, template, description…"));
    m_filter->setClearButtonEnabled(true);
    m_filter->setToolTip(Tr::tr("Type to filter; Up/Down moves through the list."));
    m_filter->installEventFilter(this);

    auto *topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(6);
    topRow->addWidget(m_filter, 1);

    m_tagStrip = new TagFilterStrip(this);
    m_tagStrip->setMaxColumns(6);

    const bool dark = CardStyle::isDark(palette());
    const auto tone = CardStyle::toneFor(dark);

    m_resultCount = new QLabel(this);
    {
        QPalette rp = m_resultCount->palette();
        rp.setColor(QPalette::WindowText, QColor(tone.textMute));
        m_resultCount->setPalette(rp);
    }

    auto *expandAll
        = new QLabel(QStringLiteral("<a href=\"#\">%1</a>").arg(Tr::tr("Expand all")), this);
    auto *collapseAll
        = new QLabel(QStringLiteral("<a href=\"#\">%1</a>").arg(Tr::tr("Collapse all")), this);

    auto *controlsRow = new QHBoxLayout;
    controlsRow->setContentsMargins(2, 0, 2, 0);
    controlsRow->setSpacing(8);
    controlsRow->addWidget(m_resultCount);
    controlsRow->addStretch(1);
    controlsRow->addWidget(expandAll);
    controlsRow->addWidget(collapseAll);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::StyledPanel);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttons->button(QDialogButtonBox::Ok);
    m_okButton->setText(isChange ? Tr::tr("Change") : Tr::tr("Add"));
    m_okButton->setEnabled(false);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(topRow);
    layout->addWidget(m_tagStrip);
    layout->addLayout(controlsRow);
    layout->addWidget(m_scroll);
    layout->addWidget(buttons);

    QMap<QString, int> tagCounts;
    for (const auto &cfg : (m_agentFactory ? m_agentFactory->configs() : m_localConfigs)) {
        if (cfg.hidden)
            continue;
        for (const QString &tag : cfg.tags)
            tagCounts[tag] += 1;
    }

    QSet<QString> preset;
    for (const QString &tag : m_presetTags)
        if (tagCounts.contains(tag))
            preset.insert(tag);
    m_tagStrip->setAvailableTags(tagCounts, preset);

    connect(m_tagStrip, &TagFilterStrip::activeTagsChanged, this, [this](const QSet<QString> &) {
        applyFilters();
    });
    connect(expandAll, &QLabel::linkActivated, this, [this](const QString &) {
        setAllExpanded(true);
    });
    connect(collapseAll, &QLabel::linkActivated, this, [this](const QString &) {
        setAllExpanded(false);
    });

    rebuild(currentName);

    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &) { applyFilters(); });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_filter->setFocus();
}

void AgentSelectionDialog::setAllExpanded(bool expanded)
{
    for (auto *section : m_sections)
        if (!section->isHidden())
            section->setExpanded(expanded);
}

bool AgentSelectionDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_filter && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Down) {
            moveSelection(1);
            return true;
        }
        if (ke->key() == Qt::Key_Up) {
            moveSelection(-1);
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

QList<ListRowCard *> AgentSelectionDialog::filteredCards() const
{
    const QString needle = m_filter ? m_filter->text().trimmed() : QString();
    const QSet<QString> activeTags = m_tagStrip ? m_tagStrip->activeTags() : QSet<QString>();
    QList<ListRowCard *> out;
    for (auto *section : m_sections)
        for (auto *card : section->cards())
            if (card->matches(needle) && card->hasAllTags(activeTags))
                out.append(card);
    return out;
}

void AgentSelectionDialog::moveSelection(int delta)
{
    const QList<ListRowCard *> cards = filteredCards();
    if (cards.isEmpty())
        return;
    const int cur = m_currentCard ? int(cards.indexOf(m_currentCard)) : -1;
    int next = cur < 0 ? (delta > 0 ? 0 : int(cards.size()) - 1) : cur + delta;
    next = qBound(0, next, int(cards.size()) - 1);
    if (next == cur)
        return;
    ListRowCard *card = cards.at(next);
    for (auto *section : m_sections) {
        if (section->cards().contains(card)) {
            section->setExpanded(true);
            break;
        }
    }
    selectCard(card);
    QPointer<ListRowCard> guarded(card);
    QTimer::singleShot(0, this, [this, guarded] {
        if (guarded)
            m_scroll->ensureWidgetVisible(guarded, 0, 40);
    });
}

void AgentSelectionDialog::selectCard(ListRowCard *card)
{
    if (m_currentCard == card)
        return;
    if (m_currentCard)
        m_currentCard->setSelected(false);
    m_currentCard = card;
    if (m_currentCard) {
        m_currentCard->setSelected(true);
        m_selectedName = m_currentCard->itemName();
    } else {
        m_selectedName.clear();
    }
    if (m_okButton)
        m_okButton->setEnabled(!m_selectedName.isEmpty());
}

void AgentSelectionDialog::rebuild(const QString &currentName)
{
    m_sections.clear();
    m_currentCard = nullptr;
    m_selectedName.clear();
    if (m_okButton)
        m_okButton->setEnabled(false);

    const auto &configs = m_agentFactory ? m_agentFactory->configs() : m_localConfigs;

    auto *content = new QWidget;
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(12, 12, 12, 12);
    contentLayout->setSpacing(6);

    QMap<QString, std::vector<const AgentConfig *>> byProvider;
    for (const auto &cfg : configs) {
        if (cfg.hidden)
            continue;
        const QString key = cfg.providerInstance.isEmpty() ? Tr::tr("(Unknown provider instance)")
                                                           : cfg.providerInstance;
        byProvider[key].push_back(&cfg);
    }

    AgentRowCard *toSelect = nullptr;
    ProviderSection *sectionToExpand = nullptr;

    for (auto it = byProvider.cbegin(); it != byProvider.cend(); ++it) {
        auto *section = new ProviderSection(it.key());
        auto sortedConfigs = it.value();
        std::sort(
            sortedConfigs.begin(),
            sortedConfigs.end(),
            [](const AgentConfig *a, const AgentConfig *b) { return a->name < b->name; });
        for (const AgentConfig *cfg : sortedConfigs) {
            auto *card = new AgentRowCard(*cfg);
            connect(card, &ListRowCard::clicked, this, [this, card]() { selectCard(card); });
            connect(card, &ListRowCard::activated, this, [this, card]() {
                selectCard(card);
                accept();
            });
            section->addCard(card);
            if (cfg->name == currentName) {
                toSelect = card;
                sectionToExpand = section;
            }
        }
        contentLayout->addWidget(section);
        m_sections.append(section);
    }

    m_emptyLabel = new QLabel(tr("No agents match the current filter."), content);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setContentsMargins(10, 24, 10, 24);
    {
        QFont ef = m_emptyLabel->font();
        ef.setItalic(true);
        m_emptyLabel->setFont(ef);
        QPalette ep = m_emptyLabel->palette();
        ep.setColor(QPalette::WindowText, ep.color(QPalette::Mid));
        m_emptyLabel->setPalette(ep);
    }
    m_emptyLabel->setVisible(false);
    contentLayout->addWidget(m_emptyLabel);
    contentLayout->addStretch(1);

    m_scroll->setWidget(content);

    if (sectionToExpand)
        sectionToExpand->setExpanded(true);

    if (toSelect) {
        selectCard(toSelect);
        QTimer::singleShot(0, this, [this, toSelect]() {
            m_scroll->ensureWidgetVisible(toSelect, 0, 60);
        });
    }

    applyFilters();
}

void AgentSelectionDialog::applyFilters()
{
    const QString needle = m_filter ? m_filter->text().trimmed() : QString();
    const QSet<QString> activeTags = m_tagStrip ? m_tagStrip->activeTags() : QSet<QString>();
    const bool filtering = !needle.isEmpty() || !activeTags.isEmpty();
    int total = 0;
    for (auto *section : m_sections) {
        const int visible = section->applyFilter(needle, activeTags);
        section->setVisible(visible > 0);
        if (filtering)
            section->setExpanded(visible > 0);
        total += visible;
    }
    if (m_currentCard && (!m_currentCard->matches(needle) || !m_currentCard->hasAllTags(activeTags)))
        selectCard(nullptr);
    if (m_emptyLabel)
        m_emptyLabel->setVisible(total == 0);
    if (m_resultCount)
        m_resultCount->setText(total == 0 ? tr("No matches") : tr("%n agent(s)", nullptr, total));

    if (m_tagStrip) {
        QMap<QString, int> liveCounts;
        for (auto *section : m_sections)
            for (auto *card : section->cards())
                if (card->matches(needle) && card->hasAllTags(activeTags))
                    for (const QString &tag : card->itemTags())
                        liveCounts[tag] += 1;
        m_tagStrip->setVisibleCounts(liveCounts);
    }
}

} // namespace QodeAssist::Settings
