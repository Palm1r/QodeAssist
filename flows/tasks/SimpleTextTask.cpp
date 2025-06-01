/*
 * Copyright (C) 2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
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

#include "SimpleTextTask.hpp"
#include <logger/Logger.hpp>

namespace QodeAssist {

SimpleTextTask::SimpleTextTask(QObject *parent)
    : BaseTask(parent)
{
    addParameter("inputText", QString("Hello World"));

    addInputPort("text_input");
    addOutputPort("text_output");
    addOutputPort("length");
    addOutputPort("completed");
}

TaskState SimpleTextTask::execute()
{
    LOG_MESSAGE("SimpleTextTask: Starting execution");

    QVariant inputValue = getInputValue("text_input");
    QString text;

    if (inputValue.isValid() && !inputValue.toString().isEmpty()) {
        text = inputValue.toString();
    } else {
        text = getParameter("inputText").toString();
    }

    LOG_MESSAGE(QString("SimpleTextTask: Processing text: %1").arg(text));

    QString processedText = text.toUpper();
    int textLength = text.length();

    setOutputValue("text_output", processedText);
    setOutputValue("length", textLength);
    setOutputValue("completed", true);

    LOG_MESSAGE(QString("SimpleTextTask: Completed. Output: %1, Length: %2")
                    .arg(processedText)
                    .arg(textLength));

    return TaskState::Success;
}

} // namespace QodeAssist
