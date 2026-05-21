// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

class QLabel;
class QVBoxLayout;

namespace QodeAssist::Settings {

class SectionBox : public QWidget
{
public:
    explicit SectionBox(const QString &title, QWidget *parent = nullptr);

    QVBoxLayout *bodyLayout() const { return m_bodyLayout; }

private:
    QLabel *m_title = nullptr;
    QWidget *m_body = nullptr;
    QVBoxLayout *m_bodyLayout = nullptr;
};

} // namespace QodeAssist::Settings
