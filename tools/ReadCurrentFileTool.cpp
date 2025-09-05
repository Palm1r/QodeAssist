#include "ReadCurrentFileTool.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <logger/Logger.hpp>

namespace QodeAssist::Tools {

QString ReadCurrentFileTool::name() const
{
    return "read_current_file";
}

QString ReadCurrentFileTool::description() const
{
    return "Read the content of the currently opened file in the editor";
}

QJsonObject ReadCurrentFileTool::getDefinition() const
{
    QJsonObject tool;
    tool["name"] = name();
    tool["description"] = description();

    QJsonObject inputSchema;
    inputSchema["type"] = "object";
    inputSchema["properties"] = QJsonObject();
    inputSchema["required"] = QJsonArray();

    tool["input_schema"] = inputSchema;
    return tool;
}

QString ReadCurrentFileTool::execute(const QJsonObject &input)
{
    Q_UNUSED(input)

    LOG_MESSAGE("excecute tool read file");
    auto editor = Core::EditorManager::currentEditor();
    if (!editor) {
        return "Error: No file is currently open in the editor";
    }

    auto document = editor->document();
    if (!document) {
        return "Error: No document available";
    }

    QString filePath = document->filePath().toFSPathString();
    QByteArray contentBytes = document->contents();
    QString fileContent = QString::fromUtf8(contentBytes);

    if (fileContent.isEmpty()) {
        return QString("File: %1\n\nThe file is empty or could not be read").arg(filePath);
    }

    return QString("File: %1\n\nContent:\n%2").arg(filePath, fileContent);
}

} // namespace QodeAssist::Tools
