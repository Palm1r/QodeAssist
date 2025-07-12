#include "TaskConnectionsModel.hpp"

namespace QodeAssist::TaskFlow {

TaskConnectionsModel::TaskConnectionsModel(Flow *flow, QObject *parent)
    : QAbstractListModel(parent)
    , m_flow(flow)
{}

int TaskConnectionsModel::rowCount(const QModelIndex &parent) const
{
    return m_flow->connections().size();
}

QVariant TaskConnectionsModel::data(const QModelIndex &index, int role) const
{
    if (role == TaskConnectionsRoles::TaskConnectionsRole)
        return QVariant::fromValue(m_flow->connections().at(index.row()));
    return QVariant();
}

QHash<int, QByteArray> TaskConnectionsModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TaskConnectionsRoles::TaskConnectionsRole] = "connectionData";
    return roles;
}

} // namespace QodeAssist::TaskFlow
