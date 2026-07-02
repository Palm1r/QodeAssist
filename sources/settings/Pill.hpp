// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QLabel>
#include <QString>

namespace QodeAssist::Settings {

class Pill : public QLabel
{
    Q_OBJECT
public:
    enum Kind : int { Neutral, Accent, On, Off, User, Tag, Active, Match };

    explicit Pill(Kind kind, const QString &text = {}, QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

private:
    void applyTheme();

    Kind m_kind;
    bool m_inApplyTheme = false;
};

} // namespace QodeAssist::Settings
