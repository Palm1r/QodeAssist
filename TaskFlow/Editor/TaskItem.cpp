#include "TaskItem.hpp"

namespace QodeAssist::TaskFlow {

TaskItem::TaskItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setObjectName("TaskItem");
}

QString TaskItem::taskId() const
{
    return m_taskId;
}

void TaskItem::setTaskId(const QString &newTaskId)
{
    if (m_taskId == newTaskId)
        return;
    m_taskId = newTaskId;
    emit taskIdChanged();
}

QString TaskItem::taskType() const
{
    return m_task ? m_task->taskType() : QString();
}

BaseTask *TaskItem::task() const
{
    return m_task;
}

void TaskItem::setTask(BaseTask *newTask)
{
    if (m_task == newTask)
        return;

    m_task = newTask;

    if (m_task) {
        m_taskId = m_task->taskId();

        // Обновляем модели портов
        m_inputPorts = new TaskPortModel(m_task->getInputPorts(), this);
        m_outputPorts = new TaskPortModel(m_task->getOutputPorts(), this);
    } else {
        m_inputPorts = nullptr;
        m_outputPorts = nullptr;
    }

    emit taskChanged();
    emit inputPortsChanged();
    emit outputPortsChanged();
    emit taskIdChanged();
    emit taskTypeChanged();
}

TaskPortModel *TaskItem::inputPorts() const
{
    return m_inputPorts;
}

TaskPortModel *TaskItem::outputPorts() const
{
    return m_outputPorts;
}

} // namespace QodeAssist::TaskFlow
