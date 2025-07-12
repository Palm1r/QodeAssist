#include "TaskConnectionItem.hpp"
#include "TaskItem.hpp"
#include "TaskPortItem.hpp"
#include <QDebug>

namespace QodeAssist::TaskFlow {

TaskConnectionItem::TaskConnectionItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setObjectName("TaskConnectionItem");
}

void TaskConnectionItem::setConnection(TaskConnection *connection)
{
    if (m_connection == connection)
        return;

    m_connection = connection;
    emit connectionChanged();

    calculatePositions();
}

void TaskConnectionItem::updatePositions()
{
    // calculatePositions();
}

void TaskConnectionItem::calculatePositions()
{
    if (!m_connection) {
        return;
    }

    // Find source task item
    QQuickItem *sourceTaskItem = findTaskItem(m_connection->sourceTask());
    QQuickItem *targetTaskItem = findTaskItem(m_connection->targetTask());

    if (!sourceTaskItem || !targetTaskItem) {
        return;
    }

    // Find port items within tasks
    QQuickItem *sourcePortItem = findPortItem(sourceTaskItem, m_connection->sourcePort());
    QQuickItem *targetPortItem = findPortItem(targetTaskItem, m_connection->targetPort());

    if (!sourcePortItem || !targetPortItem) {
        return;
    }

    // Calculate global positions
    QPointF sourceGlobal
        = sourcePortItem
              ->mapToItem(parentItem(), sourcePortItem->width() / 2, sourcePortItem->height() / 2);
    QPointF targetGlobal
        = targetPortItem
              ->mapToItem(parentItem(), targetPortItem->width() / 2, targetPortItem->height() / 2);

    if (m_startPoint != sourceGlobal) {
        m_startPoint = sourceGlobal;
        emit startPointChanged();
    }

    if (m_endPoint != targetGlobal) {
        m_endPoint = targetGlobal;
        emit endPointChanged();
    }
}

QQuickItem *TaskConnectionItem::findTaskItem(BaseTask *task)
{
    for (const QVariant &item : m_taskItems) {
        QQuickItem *taskItem = qvariant_cast<QQuickItem *>(item);
        if (!taskItem)
            continue;

        QVariant taskProp = taskItem->property("task");
        if (taskProp.isValid() && taskProp.value<BaseTask *>() == task) {
            return taskItem;
        }
    }
    return nullptr;
}

QQuickItem *TaskConnectionItem::findTaskItemRecursive(QQuickItem *item, BaseTask *task)
{
    // Проверяем objectName и task property
    if (item->objectName() == "TaskItem") {
        QVariant taskProp = item->property("task");
        if (taskProp.isValid()) {
            BaseTask *itemTask = taskProp.value<BaseTask *>();
            if (itemTask == task) {
                return item;
            }
        }
    }

    // Рекурсивно ищем в детях
    auto children = item->childItems();

    for (QQuickItem *child : children) {
        if (QQuickItem *found = findTaskItemRecursive(child, task)) {
            return found;
        }
    }

    return nullptr;
}

QQuickItem *TaskConnectionItem::findPortItem(QQuickItem *taskItem, TaskPort *port)
{
    std::function<QQuickItem *(QQuickItem *)> findPortRecursive =
        [&](QQuickItem *item) -> QQuickItem * {
        // Проверяем objectName и port property
        if (item->objectName() == "TaskPortItem") {
            QVariant portProp = item->property("port");
            if (portProp.isValid()) {
                TaskPort *itemPort = portProp.value<TaskPort *>();
                if (itemPort == port) {
                    return item;
                }
            }
        }

        // Рекурсивно ищем в детях
        for (QQuickItem *child : item->childItems()) {
            if (QQuickItem *found = findPortRecursive(child)) {
                return found;
            }
        }
        return nullptr;
    };

    return findPortRecursive(taskItem);
}

QVariantList TaskConnectionItem::taskItems() const
{
    return m_taskItems;
}

void TaskConnectionItem::setTaskItems(const QVariantList &newTaskItems)
{
    if (m_taskItems == newTaskItems)
        return;
    m_taskItems = newTaskItems;
    emit taskItemsChanged();

    calculatePositions();
}

} // namespace QodeAssist::TaskFlow
