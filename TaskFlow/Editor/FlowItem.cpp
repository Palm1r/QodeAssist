#include "FlowItem.hpp"

namespace QodeAssist::TaskFlow {

FlowItem::FlowItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    connect(this, &QQuickItem::childrenChanged, this, [this]() { updateFlowLayout(); });
}

QString FlowItem::flowId() const
{
    if (!m_flow)
        return {"no flow"};
    return m_flow->flowId();
}

void FlowItem::setFlowId(const QString &newFlowId)
{
    if (m_flow->flowId() == newFlowId)
        return;
    m_flow->setFlowId(newFlowId);
    emit flowIdChanged();
}

Flow *FlowItem::flow() const
{
    return m_flow;
}

void FlowItem::setFlow(Flow *newFlow)
{
    if (m_flow == newFlow)
        return;
    m_flow = newFlow;
    emit flowChanged();
    emit flowIdChanged();
    qDebug() << "FlowItem::setFlow" << m_flow->flowId() << newFlow;

    m_taskModel = new TaskModel(m_flow, this);
    m_connectionsModel = new TaskConnectionsModel(m_flow, this);

    emit taskModelChanged();
    emit connectionsModelChanged();
}

TaskModel *FlowItem::taskModel() const
{
    return m_taskModel;
}

TaskConnectionsModel *FlowItem::connectionsModel() const
{
    return m_connectionsModel;
}

QVariantList FlowItem::taskItems() const
{
    return m_taskItems;
}

void FlowItem::setTaskItems(const QVariantList &newTaskItems)
{
    qDebug() << "FlowItem::setTaskItems" << newTaskItems;
    if (m_taskItems == newTaskItems)
        return;
    m_taskItems = newTaskItems;
    emit taskItemsChanged();
}

void FlowItem::updateFlowLayout()
{
    auto allItems = this->childItems();

    for (auto child : allItems) {
        if (child->objectName() == QString("TaskItem")) {
            qDebug() << "Found TaskItem:" << child;
            auto taskItem = qobject_cast<TaskItem *>(child);
            m_taskItemsList.insert(taskItem, taskItem->task());
        }

        if (child->objectName() == QString("TaskConnectionItem")) {
            qDebug() << "Found TaskConnectionItem:" << child;
            auto connectionItem = qobject_cast<TaskConnectionItem *>(child);
            m_taskConnectionsList.insert(connectionItem, connectionItem->connection());
        }
    }
}

} // namespace QodeAssist::TaskFlow
