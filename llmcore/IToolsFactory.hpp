#pragma once

#include "ITool.hpp"
#include <QJsonArray>
#include <QList>

namespace QodeAssist::LLMCore {

class IToolsFactory
{
public:
    virtual ~IToolsFactory() = default;
    virtual QList<ITool *> getAvailableTools() const = 0;
    virtual ITool *getToolByName(const QString &name) const = 0;
    virtual QJsonArray getToolsDefinitions() const = 0;
};

} // namespace QodeAssist::LLMCore
