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

#include "DemoTasks.hpp"
#include <logger/Logger.hpp>
#include <QDateTime>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QThread>
#include <QTimer>

namespace QodeAssist {

Task1::Task1(QObject *parent)
    : BaseTask(parent)
{
    addParameter("filePath", QString());

    addOutputPort("file_path");
    addOutputPort("file_size");
    addOutputPort("last_modified");
    addOutputPort("completed");
    addOutputPort("error");
}

QJsonObject Task1::toJson() const
{
    return BaseTask::toJson();
}

bool Task1::fromJson(const QJsonObject &json)
{
    return BaseTask::fromJson(json);
}

TaskState Task1::execute()
{
    QString filePath = getParameter("filePath").toString();

    LOG_MESSAGE(QString("Task1: Starting file processing for '%1'").arg(filePath));

    if (filePath.isEmpty()) {
        LOG_MESSAGE("Task1: File path is empty");
        setOutputValue("error", QString("File path is empty"));
        return TaskState::Failed;
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        LOG_MESSAGE(QString("Task1: File does not exist: %1").arg(filePath));
        setOutputValue("error", QString("File does not exist: %1").arg(filePath));
        return TaskState::Failed;
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    for (int i = 0; i < 5; ++i) {
        timer.start(500);
        loop.exec();
        LOG_MESSAGE(QString("Task1: Processing step %1/5").arg(i + 1));
    }

    // Set output data through ports
    setOutputValue("file_path", filePath);
    setOutputValue("file_size", fileInfo.size());
    setOutputValue("last_modified", fileInfo.lastModified());
    setOutputValue("completed", true);

    LOG_MESSAGE(QString("Task1: Successfully processed file '%1' (size: %2 bytes)")
                    .arg(filePath)
                    .arg(fileInfo.size()));

    return TaskState::Success;
}

Task2::Task2(QObject *parent)
    : BaseTask(parent)
{
    addInputPort("file_path");
    addInputPort("file_size");
    addInputPort("last_modified");
    addInputPort("completed");

    addOutputPort("analysis_result");
    addOutputPort("file_category");
    addOutputPort("analyzed_file");
    addOutputPort("analysis_timestamp");
    addOutputPort("completed");
    addOutputPort("error");
}

QJsonObject Task2::toJson() const
{
    return BaseTask::toJson();
}

bool Task2::fromJson(const QJsonObject &json)
{
    return BaseTask::fromJson(json);
}

TaskState Task2::execute()
{
    LOG_MESSAGE("Task2: Starting analysis");

    QVariant completedValue = getInputValue("completed");
    if (!completedValue.isValid() || !completedValue.toBool()) {
        LOG_MESSAGE("Task2: Missing or invalid input data from Task1");
        setOutputValue("error", "Missing input data from Task1");
        return TaskState::Failed;
    }

    QString filePath = getInputValue("file_path").toString();
    qint64 fileSize = getInputValue("file_size").toLongLong();
    QDateTime lastModified = getInputValue("last_modified").toDateTime();

    LOG_MESSAGE(QString("Task2: Analyzing file '%1' (size: %2)").arg(filePath).arg(fileSize));

    QString analysisResult;
    QString category;

    if (fileSize > 1024 * 1024) {
        analysisResult = "Large file detected";
        category = "large";
    } else if (fileSize > 1024) {
        analysisResult = "Medium file detected";
        category = "medium";
    } else {
        analysisResult = "Small file detected";
        category = "small";
    }

    setOutputValue("analysis_result", analysisResult);
    setOutputValue("file_category", category);
    setOutputValue("analyzed_file", filePath);
    setOutputValue("analysis_timestamp", QDateTime::currentDateTime());
    setOutputValue("completed", true);

    LOG_MESSAGE(QString("Task2: Analysis completed. Result: %1 for file '%2'")
                    .arg(analysisResult, filePath));

    return TaskState::Success;
}

} // namespace QodeAssist
