#include "TaskPortModel.hpp"
#include "TaskPort.hpp"

namespace QodeAssist::TaskFlow {

TaskPortModel::TaskPortModel(const QList<TaskPort *> &ports, QObject *parent)
    : QAbstractListModel(parent)
    , m_ports(ports)
{}

int TaskPortModel::rowCount(const QModelIndex &parent) const
{
    return m_ports.size();
}

QVariant TaskPortModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_ports.size())
        return QVariant();

    switch (role) {
    case TaskPortRoles::TaskPortNameRole:
        return m_ports.at(index.row())->name();
    case TaskPortRoles::TaskPortDataRole:
        return QVariant::fromValue(m_ports.at(index.row()));
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> TaskPortModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TaskPortRoles::TaskPortNameRole] = "taskPortName";
    roles[TaskPortRoles::TaskPortDataRole] = "taskPortData";
    return roles;
}

} // namespace QodeAssist::TaskFlow
