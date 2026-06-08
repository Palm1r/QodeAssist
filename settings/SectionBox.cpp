// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "SectionBox.hpp"

#include <QLabel>
#include <QVBoxLayout>

namespace QodeAssist::Settings {

SectionBox::SectionBox(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    m_title = new QLabel(title, this);
    QFont tf = m_title->font();
    tf.setBold(true);
    m_title->setFont(tf);

    m_body = new QWidget(this);
    m_bodyLayout = new QVBoxLayout(m_body);
    m_bodyLayout->setContentsMargins(0, 0, 0, 0);
    m_bodyLayout->setSpacing(4);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 4, 0, 4);
    outer->setSpacing(4);
    outer->addWidget(m_title);
    outer->addWidget(m_body, 1);
}

} // namespace QodeAssist::Settings
