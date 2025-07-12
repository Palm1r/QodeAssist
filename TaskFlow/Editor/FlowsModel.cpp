#include "FlowsModel.hpp"

#include "FlowManager.hpp"

namespace QodeAssist::TaskFlow {

FlowsModel::FlowsModel(FlowManager *flowManager, QObject *parent)
    : QAbstractListModel(parent)
    , m_flowManager(flowManager)
{
    connect(m_flowManager, &FlowManager::flowAdded, this, &FlowsModel::onFlowAdded);
}

int FlowsModel::rowCount(const QModelIndex &parent) const
{
    return m_flowManager->flows().size();
}

QVariant FlowsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !m_flowManager || index.row() >= m_flowManager->flows().size())
        return QVariant();

    const auto flows = m_flowManager->flows().values();

    switch (role) {
    case FlowRoles::FlowIdRole:
        return flows.at(index.row())->flowId();
    case FlowRoles::FlowDataRole:
        return QVariant::fromValue(flows.at(index.row()));
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> FlowsModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[FlowRoles::FlowIdRole] = "flowId";
    roles[FlowRoles::FlowDataRole] = "flowData";
    return roles;
}

void FlowsModel::onFlowAdded(const QString &flowId)
{
    // qDebug() << "FlowsModel::Flow added: " << flowId;
    // int newIndex = m_flowManager->flows().size();
    // beginInsertRows(QModelIndex(), newIndex, newIndex);
    // endInsertRows();
}

void FlowsModel::onFlowRemoved(const QString &flowId) {}

} // namespace QodeAssist::TaskFlow
