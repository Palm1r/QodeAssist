/*
 * Copyright (C) 2023 The Qt Company Ltd.
 * Copyright (C) 2024 Petr Mironychev
 *
 * This file is part of Qode Assist.
 *
 * The Qt Company portions:
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0
 *
 * Petr Mironychev portions:
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QTextBlock>
#include <texteditor/basehoverhandler.h>

namespace QodeAssist {

class QodeAssistHoverHandler : public TextEditor::BaseHoverHandler
{
public:
    QodeAssistHoverHandler() = default;

protected:
    void identifyMatch(TextEditor::TextEditorWidget *editorWidget,
                       int pos,
                       ReportPriority report) final;
    void operateTooltip(TextEditor::TextEditorWidget *editorWidget, const QPoint &point) final;

private:
    QTextBlock m_block;
};

} // namespace QodeAssist
