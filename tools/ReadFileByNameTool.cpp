#include "ReadFileByNameTool.hpp"

#include <coreplugin/documentmanager.h>
#include <logger/Logger.hpp>
#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace QodeAssist::Tools {

QString ReadFileByNameTool::name() const
{
    return "read_file_by_name";
}

QString ReadFileByNameTool::description() const
{
    return "Read the content of a specific file in the project by filename";
}

QJsonObject ReadFileByNameTool::getDefinition() const
{
    QJsonObject tool;
    tool["name"] = name();
    tool["description"] = description();

    QJsonObject inputSchema;
    inputSchema["type"] = "object";

    QJsonObject properties;
    QJsonObject filenameProperty;
    filenameProperty["type"] = "string";
    filenameProperty["description"] = "The filename or relative path to read";
    properties["filename"] = filenameProperty;

    inputSchema["properties"] = properties;

    QJsonArray required;
    required.append("filename");
    inputSchema["required"] = required;

    tool["input_schema"] = inputSchema;

    // Добавим логирование для отладки
    LOG_MESSAGE(QString("ReadFileByNameTool definition: %1")
                    .arg(QJsonDocument(tool).toJson(QJsonDocument::Compact)));

    return tool;
}

QString ReadFileByNameTool::execute(const QJsonObject &input)
{
    emit toolStarted(name());

    LOG_MESSAGE(QString("ReadFileByNameTool: execute with input: %1")
                    .arg(QJsonDocument(input).toJson(QJsonDocument::Compact)));

    QString fileName = input["filename"].toString();
    if (fileName.isEmpty()) {
        return "Error: filename parameter is required";
    }

    if (!input.contains("filename")) {
        QString error = "Error: filename parameter is required";
        LOG_MESSAGE(error);
        emit toolFailed(name(), error);
        return error;
    }

    QString filename = input["filename"].toString();
    if (filename.isEmpty()) {
        QString error = "Error: filename cannot be empty";
        LOG_MESSAGE(error);
        emit toolFailed(name(), error);
        return error;
    }

    LOG_MESSAGE(QString("ReadFileByNameTool: Requested to read file: %1").arg(filename));

    QString filePath = findFileInProject(fileName);
    if (filePath.isEmpty()) {
        return QString("Error: File '%1' not found in the current project").arg(fileName);
    }

    QString content = readFileContent(filePath);
    if (content.isNull()) {
        return QString("Error: Could not read file '%1'").arg(filePath);
    }

    if (content.isEmpty()) {
        return QString("File: %1\n\nThe file is empty").arg(filePath);
    }

    QString result = QString("File: %1\n\nContent:\n%2").arg(filePath, content);
    LOG_MESSAGE(QString("ReadFileByNameTool: Result: %1").arg(result));
    emit toolCompleted(name(), result);
    return result;
}

QString ReadFileByNameTool::findFileInProject(const QString &fileName) const
{
    auto project = ProjectExplorer::ProjectManager::startupProject();
    if (!project) {
        LOG_MESSAGE("No startup project found");
        return QString();
    }

    // Get all project files
    Utils::FilePaths projectFiles = project->files(ProjectExplorer::Project::SourceFiles);

    // First try exact match
    for (const auto &projectFile : projectFiles) {
        QFileInfo fileInfo(projectFile.path());
        if (fileInfo.fileName() == fileName) {
            return projectFile.path();
        }
    }

    // Then try relative path match
    for (const auto &projectFile : projectFiles) {
        if (projectFile.endsWith(fileName)) {
            return projectFile.path();
        }
    }

    // Finally try contains match (partial filename)
    for (const auto &projectFile : projectFiles) {
        QFileInfo fileInfo(projectFile.path());
        if (fileInfo.fileName().contains(fileName, Qt::CaseInsensitive)) {
            return projectFile.path();
        }
    }

    return QString();
}

QString ReadFileByNameTool::readFileContent(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_MESSAGE(QString("Could not open file: %1").arg(filePath));
        return QString();
    }

    QTextStream stream(&file);
    stream.setAutoDetectUnicode(true);
    QString content = stream.readAll();

    file.close();
    return content;
}

} // namespace QodeAssist::Tools
