// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QHash>
#include <QMap>
#include <QSet>
#include <QString>
#include <QWidget>

class QLabel;
class QVBoxLayout;

namespace QodeAssist::Settings {

class TagChip;

class TagFilterStrip : public QWidget
{
    Q_OBJECT
public:
    explicit TagFilterStrip(QWidget *parent = nullptr);

    void setAvailableTags(const QMap<QString, int> &countsByTag);
    void setAvailableTags(const QMap<QString, int> &countsByTag, const QSet<QString> &activeTags);
    void setVisibleCounts(const QMap<QString, int> &countsByTag);
    void setMaxColumns(int columns);
    const QSet<QString> &activeTags() const { return m_activeTags; }
    void toggleTag(const QString &tag);

signals:
    void activeTagsChanged(const QSet<QString> &tags);

protected:
    void changeEvent(QEvent *event) override;

private:
    void rebuild();
    void refreshActiveStates();
    void applyTheme();

    QMap<QString, int> m_counts;
    QSet<QString> m_activeTags;
    int m_maxColumns = 3;
    QVBoxLayout *m_layout = nullptr;
    QHash<QString, TagChip *> m_chipByTag;
    QLabel *m_clearLink = nullptr;
    bool m_inApplyTheme = false;
};

} // namespace QodeAssist::Settings
