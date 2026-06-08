// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QDialog>
#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QPalette>
#include <QStringList>
#include <vector>

#include <AgentConfig.hpp>

class QLineEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

namespace QodeAssist {
class AgentFactory;
}

namespace QodeAssist::Settings {

namespace CardStyle {

struct Tone
{
    QString bg;
    QString hoverBg;
    QString selectedBg;
    QString selectedBd;
    QString cardBd;
    QString textSoft;
    QString textMute;
    QString textFaint;
};

inline bool isDark(const QPalette &p)
{
    return p.color(QPalette::Window).lightness() < 128;
}

inline Tone toneFor(bool dark)
{
    return dark
        ? Tone{"#333333", "#3a3a3a", "#2a4566", "#3a6fb7", "#4a4a4a",
               "#c2c2c2", "#9a9a9a", "#7a7a7a"}
        : Tone{"#f6f6f6", "#ececec", "#cfe1f7", "#3a6fb7", "#bdbdbd",
               "#3a3a3a", "#5a5a5a", "#8a8a8a"};
}

inline QFont monoFont(int pixelSize)
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

inline void applySectionFont(QLabel *l)
{
    QFont f = l->font();
    f.setPointSizeF(f.pointSizeF() * 0.85);
    f.setBold(true);
    f.setCapitalization(QFont::AllUppercase);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    l->setFont(f);
    QPalette p = l->palette();
    p.setColor(QPalette::WindowText, p.color(QPalette::Mid));
    l->setPalette(p);
}

} // namespace CardStyle

class ListRowCard : public QFrame
{
    Q_OBJECT
public:
    QString itemName() const { return m_itemName; }
    bool matches(const QString &needle) const;
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }

signals:
    void clicked();
    void activated();

protected:
    explicit ListRowCard(QWidget *parent = nullptr);

    void setItemName(const QString &name) { m_itemName = name; }
    void buildSearchHaystack(const QStringList &parts);

    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void applyTheme();

    QString m_itemName;
    QString m_searchHaystack;
    bool m_selected = false;
    bool m_hover = false;
    bool m_inApplyTheme = false;
};

class AgentRowCard : public ListRowCard
{
    Q_OBJECT
public:
    explicit AgentRowCard(const AgentConfig &cfg, QWidget *parent = nullptr);

    QString agentName() const { return itemName(); }
};

class ProviderSection : public QWidget
{
    Q_OBJECT
public:
    explicit ProviderSection(const QString &name, QWidget *parent = nullptr);

    void addCard(ListRowCard *card);
    void setExpanded(bool expanded);
    bool isExpanded() const { return m_expanded; }
    const QList<ListRowCard *> &cards() const { return m_cards; }

    int applyFilter(const QString &needle); // returns number of visible cards

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QFrame *m_header = nullptr;
    QLabel *m_arrow = nullptr;
    QLabel *m_label = nullptr;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QList<ListRowCard *> m_cards;
    bool m_expanded = true;
};

class AgentSelectionDialog : public QDialog
{
    Q_OBJECT
public:
    AgentSelectionDialog(
        const std::vector<AgentConfig> &configs,
        const QString &currentName,
        AgentFactory *agentFactory = nullptr,
        QWidget *parent = nullptr);

    QString selectedName() const { return m_selectedName; }

private:
    void rebuild(const QString &currentName);
    void selectCard(ListRowCard *card);
    void applyFilter(const QString &needle);

    QLineEdit *m_filter = nullptr;
    QScrollArea *m_scroll = nullptr;
    QPushButton *m_okButton = nullptr;
    ListRowCard *m_currentCard = nullptr;
    QList<ProviderSection *> m_sections;
    QString m_selectedName;

    AgentFactory *m_agentFactory = nullptr;
    std::vector<AgentConfig> m_localConfigs; // fallback when no factory
};

} // namespace QodeAssist::Settings
