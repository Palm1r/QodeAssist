#include "ToolsFactory.hpp"

#include <QJsonObject>
#include "logger/Logger.hpp"

namespace QodeAssist::Tools {

ToolsFactory::ToolsFactory()
    : m_readCurrentFileTool(nullptr)
{
    registerTools();
}

ToolsFactory::~ToolsFactory()
{
    qDeleteAll(m_tools);
}

void ToolsFactory::registerTools()
{
    // Register available tools
    m_readCurrentFileTool = new ReadCurrentFileTool();
    registerTool(m_readCurrentFileTool);

    m_readFileByNameTool = new ReadFileByNameTool();
    registerTool(m_readFileByNameTool);

    // Future tools can be registered here:
    // m_writeFileTool = new WriteFileTool();
    // registerTool(m_writeFileTool);

    LOG_MESSAGE(QString("Registered %1 tools").arg(m_tools.size()));
}

void ToolsFactory::registerTool(LLMCore::ITool *tool)
{
    if (!tool) {
        LOG_MESSAGE("Warning: Attempted to register null tool");
        return;
    }

    const QString toolName = tool->name();
    if (m_toolsByName.contains(toolName)) {
        LOG_MESSAGE(QString("Warning: Tool '%1' already registered, replacing").arg(toolName));
    }

    m_tools.append(tool);
    m_toolsByName.insert(toolName, tool);
}

QList<LLMCore::ITool *> ToolsFactory::getAvailableTools() const
{
    return m_tools;
}

LLMCore::ITool *ToolsFactory::getToolByName(const QString &name) const
{
    return m_toolsByName.value(name, nullptr);
}

QJsonArray ToolsFactory::getToolsDefinitions() const
{
    QJsonArray toolsArray;

    for (const auto *tool : m_tools) {
        if (tool) {
            toolsArray.append(tool->getDefinition());
        }
    }

    return toolsArray;
}

} // namespace QodeAssist::Tools
