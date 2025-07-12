#pragma once

#include <QAbstractListModel>

#include <BaseTask.hpp>

namespace QodeAssist::TaskFlow {

class TaskPortModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum TaskPortRoles { TaskPortNameRole = Qt::UserRole, TaskPortDataRole };

    TaskPortModel(const QList<TaskPort *> &ports, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<TaskPort *> m_ports;
};

} // namespace QodeAssist::TaskFlow
