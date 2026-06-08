// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QString>

class QGridLayout;
class QHBoxLayout;
class QLabel;
class QLayout;
class QWidget;

namespace QodeAssist::Settings {

void applyMutedSmallCaps(QLabel *label);

QLabel *makeSectionHeader(const QString &title, QWidget *parent);

QLabel *makeHintLabel(const QString &text, QWidget *parent = nullptr);

QHBoxLayout *singleField(QWidget *w);

class FormBuilder
{
public:
    explicit FormBuilder(QGridLayout *grid, int startRow = 0);

    FormBuilder &row(const QString &label, QLayout *value, const QString &hint = {});
    FormBuilder &row(const QString &label, QWidget *value, const QString &hint = {});

    int currentRow() const { return m_row; }

private:
    QGridLayout *m_grid;
    int m_row;
};

} // namespace QodeAssist::Settings
