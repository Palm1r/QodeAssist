#include "TaskItem.hpp"

namespace QodeAssist::FlowEditor {

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
    emit taskIdChanged();
}

QString TaskItem::taskType() const
{
    return m_task->getTaskType();
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
    m_taskId = m_task->taskId();
    m_inputPorts = new TaskPortModel(m_task->getInputPorts(), this);
    m_outputPorts = new TaskPortModel(m_task->getOutputPorts(), this);
    m_parameters = new TaskParametersModel(m_task, this);

    emit taskChanged();
    emit inputPortsChanged();
    emit outputPortsChanged();
    emit parametersChanged();
    emit taskIdChanged();
}

TaskPortModel *TaskItem::inputPorts() const
{
    return m_inputPorts;
}

TaskPortModel *TaskItem::outputPorts() const
{
    return m_outputPorts;
}

TaskParametersModel *TaskItem::parameters() const
{
    return m_parameters;
}

} // namespace QodeAssist::FlowEditor
