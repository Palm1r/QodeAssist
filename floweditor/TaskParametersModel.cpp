#include "TaskParametersModel.hpp"

namespace QodeAssist::FlowEditor {

TaskParametersModel::TaskParametersModel(BaseTask *task, QObject *parent)
    : QAbstractListModel(parent)
    , m_task(task)
{}

int TaskParametersModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    return m_task->parameters().size();
}

QVariant TaskParametersModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    switch (role) {
    case ParameterRoles::ParamKeyRole:
        return m_task->parameters().keys().at(index.row());
    case ParameterRoles::ParamValueRole:
        return m_task->parameters().values().at(index.row());
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> TaskParametersModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ParameterRoles::ParamKeyRole] = "paramKey";
    roles[ParameterRoles::ParamValueRole] = "paramValue";
    return roles;
}

} // namespace QodeAssist::FlowEditor
