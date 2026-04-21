// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

namespace QodeAssist {

class CompletionHintWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CompletionHintWidget(QWidget *parent = nullptr, int fontSize = 12);
    ~CompletionHintWidget() override = default;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QColor m_accentColor;
    bool m_isHovered;
};

} // namespace QodeAssist

