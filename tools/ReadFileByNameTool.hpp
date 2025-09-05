#pragma once

#include "llmcore/ITool.hpp"

namespace QodeAssist::Tools {

class ReadFileByNameTool : public LLMCore::ITool
{
public:
    QString name() const override;
    QString description() const override;
    QJsonObject getDefinition() const override;
    QString execute(const QJsonObject &input = QJsonObject()) override;

private:
    QString findFileInProject(const QString &fileName) const;
    QString readFileContent(const QString &filePath) const;
};

} // namespace QodeAssist::Tools
