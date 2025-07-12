#pragma once

#include "TaskConnection.hpp"
#include <QPointF>
#include <QQuickItem>

namespace QodeAssist::TaskFlow {

class TaskConnectionItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QPointF startPoint READ startPoint NOTIFY startPointChanged)
    Q_PROPERTY(QPointF endPoint READ endPoint NOTIFY endPointChanged)
    Q_PROPERTY(
        TaskConnection *connection READ connection WRITE setConnection NOTIFY connectionChanged)

    Q_PROPERTY(QVariantList taskItems READ taskItems WRITE setTaskItems NOTIFY taskItemsChanged)

public:
    TaskConnectionItem(QQuickItem *parent = nullptr);

    QPointF startPoint() const { return m_startPoint; }
    QPointF endPoint() const { return m_endPoint; }

    TaskConnection *connection() const { return m_connection; }
    void setConnection(TaskConnection *connection);

    Q_INVOKABLE void updatePositions();

    QVariantList taskItems() const;
    void setTaskItems(const QVariantList &newTaskItems);

signals:
    void startPointChanged();
    void endPointChanged();
    void connectionChanged();

    void taskItemsChanged();

private:
    void calculatePositions();
    QQuickItem *findTaskItem(BaseTask *task);
    QQuickItem *findTaskItemRecursive(QQuickItem *item, BaseTask *task);
    QQuickItem *findPortItem(QQuickItem *taskItem, TaskPort *port);

private:
    TaskConnection *m_connection = nullptr;
    QPointF m_startPoint;
    QPointF m_endPoint;
    QVariantList m_taskItems;
};

} // namespace QodeAssist::TaskFlow
