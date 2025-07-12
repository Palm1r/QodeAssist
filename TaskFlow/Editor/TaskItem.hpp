#pragma once

#include <QQuickItem>

#include "TaskPortModel.hpp"
#include <BaseTask.hpp>
#include <TaskPort.hpp>

namespace QodeAssist::TaskFlow {

class TaskItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString taskId READ taskId WRITE setTaskId NOTIFY taskIdChanged)
    Q_PROPERTY(QString taskType READ taskType NOTIFY taskTypeChanged)
    Q_PROPERTY(BaseTask *task READ task WRITE setTask NOTIFY taskChanged)
    Q_PROPERTY(TaskPortModel *inputPorts READ inputPorts NOTIFY inputPortsChanged)
    Q_PROPERTY(TaskPortModel *outputPorts READ outputPorts NOTIFY outputPortsChanged)

public:
    TaskItem(QQuickItem *parent = nullptr);

    QString taskId() const;
    void setTaskId(const QString &newTaskId);
    QString taskType() const;

    BaseTask *task() const;
    void setTask(BaseTask *newTask);

    TaskPortModel *inputPorts() const;
    TaskPortModel *outputPorts() const;

signals:
    void taskIdChanged();
    void taskTypeChanged();
    void taskChanged();
    void inputPortsChanged();
    void outputPortsChanged();

private:
    QString m_taskId;
    BaseTask *m_task = nullptr;
    TaskPortModel *m_inputPorts = nullptr;
    TaskPortModel *m_outputPorts = nullptr;
};

} // namespace QodeAssist::TaskFlow
