#pragma once

#include "ReadCurrentFileTool.hpp"
#include "ReadFileByNameTool.hpp"
#include "llmcore/IToolsFactory.hpp"

namespace QodeAssist::Tools {

class ToolsFactory : public LLMCore::IToolsFactory
{
public:
    ToolsFactory();
    ~ToolsFactory();

    QList<LLMCore::ITool *> getAvailableTools() const override;
    LLMCore::ITool *getToolByName(const QString &name) const override;
    QJsonArray getToolsDefinitions() const override;

private:
    void registerTools();
    void registerTool(LLMCore::ITool *tool);

    QList<LLMCore::ITool *> m_tools;
    QHash<QString, LLMCore::ITool *> m_toolsByName;

    // Tool instances
    ReadCurrentFileTool *m_readCurrentFileTool;
    ReadFileByNameTool *m_readFileByNameTool;
};

} // namespace QodeAssist::Tools
