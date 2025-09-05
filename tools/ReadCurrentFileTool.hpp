#pragma once

#include "llmcore/ITool.hpp"

namespace QodeAssist::Tools {

class ReadCurrentFileTool : public LLMCore::ITool
{
public:
    QString name() const override;
    QString description() const override;
    QJsonObject getDefinition() const override;
    QString execute(const QJsonObject &input = QJsonObject()) override;
};

} // namespace QodeAssist::Tools
