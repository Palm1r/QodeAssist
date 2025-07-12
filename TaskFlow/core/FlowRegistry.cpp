#include "FlowRegistry.hpp"
#include "Logger.hpp"

namespace QodeAssist::TaskFlow {

FlowRegistry::FlowRegistry(QObject *parent)
    : QObject(parent)
{}

void FlowRegistry::registerFlow(const QString &flowType, FlowCreator creator)
{
    m_flowCreators[flowType] = creator;
    LOG_MESSAGE(QString("FlowRegistry: Registered flow type '%1'").arg(flowType));
}

Flow *FlowRegistry::createFlow(const QString &flowType, FlowManager *flowManager) const
{
    LOG_MESSAGE(QString("Trying to create flow: %1").arg(flowType));

    if (m_flowCreators.contains(flowType)) {
        LOG_MESSAGE(QString("Found creator for flow type: %1").arg(flowType));
        try {
            Flow *flow = m_flowCreators[flowType](flowManager);
            if (flow) {
                LOG_MESSAGE(QString("Successfully created flow: %1").arg(flowType));
                return flow;
            }
        } catch (...) {
            LOG_MESSAGE(QString("Exception while creating flow of type: %1").arg(flowType));
        }
    } else {
        LOG_MESSAGE(QString("No creator found for flow type: %1").arg(flowType));
    }

    return nullptr;
}

QStringList FlowRegistry::getAvailableTypes() const
{
    return m_flowCreators.keys();
}

} // namespace QodeAssist::TaskFlow
