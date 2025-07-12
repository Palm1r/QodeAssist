#include "TaskModel.hpp"

namespace QodeAssist::TaskFlow {

TaskModel::TaskModel(Flow *flow, QObject *parent)
    : QAbstractListModel(parent)
    , m_flow(flow)
{}

int TaskModel::rowCount(const QModelIndex &parent) const
{
    return m_flow->tasks().size();
}

QVariant TaskModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !m_flow || index.row() >= m_flow->tasks().size())
        return QVariant();

    const auto &task = m_flow->tasks().values();

    switch (role) {
    case TaskRoles::TaskIdRole:
        return task.at(index.row())->taskId();
    case TaskRoles::TaskDataRole:
        return QVariant::fromValue(task.at(index.row()));
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> TaskModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TaskRoles::TaskIdRole] = "taskId";
    roles[TaskRoles::TaskDataRole] = "taskData";
    return roles;
}

} // namespace QodeAssist::TaskFlow
