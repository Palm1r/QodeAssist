#pragma once

#include <QAbstractListModel>

#include "tasks/BaseTask.hpp"

namespace QodeAssist::FlowEditor {

class TaskParametersModel : public QAbstractListModel
{
public:
    enum ParameterRoles { ParamKeyRole = Qt::UserRole, ParamValueRole };

    TaskParametersModel(BaseTask *task, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    BaseTask *m_task;
};

} // namespace QodeAssist::FlowEditor
