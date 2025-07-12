#pragma once

#include <QQuickItem>

#include "TaskConnectionItem.hpp"
#include "TaskConnectionsModel.hpp"
#include "TaskItem.hpp"
#include "TaskModel.hpp"
#include <Flow.hpp>
#include <TaskConnection.hpp>

namespace QodeAssist::TaskFlow {

class FlowItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString flowId READ flowId WRITE setFlowId NOTIFY flowIdChanged)
    Q_PROPERTY(Flow *flow READ flow WRITE setFlow NOTIFY flowChanged)
    Q_PROPERTY(TaskModel *taskModel READ taskModel NOTIFY taskModelChanged)
    Q_PROPERTY(
        TaskConnectionsModel *connectionsModel READ connectionsModel NOTIFY connectionsModelChanged)
    Q_PROPERTY(QVariantList taskItems READ taskItems WRITE setTaskItems NOTIFY taskItemsChanged)

public:
    explicit FlowItem(QQuickItem *parent = nullptr);

    QString flowId() const;
    void setFlowId(const QString &newFlowId);

    Flow *flow() const;
    void setFlow(Flow *newFlow);

    TaskModel *taskModel() const;

    TaskConnectionsModel *connectionsModel() const;

    QVariantList taskItems() const;
    void setTaskItems(const QVariantList &newTaskItems);

    void updateFlowLayout();

signals:
    void flowIdChanged();
    void flowChanged();
    void taskModelChanged();
    void connectionsModelChanged();
    void taskItemsChanged();

private:
    Flow *m_flow = nullptr;
    TaskModel *m_taskModel = nullptr;
    TaskConnectionsModel *m_connectionsModel = nullptr;
    QVariantList m_taskItems;

    QHash<TaskItem *, BaseTask *> m_taskItemsList;
    QHash<TaskConnectionItem *, TaskConnection *> m_taskConnectionsList;
};

} // namespace QodeAssist::TaskFlow
