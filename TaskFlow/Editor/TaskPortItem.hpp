#pragma once

#include <TaskPort.hpp>
#include <QQuickItem>

namespace QodeAssist::TaskFlow {

class TaskPortItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(TaskPort *port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString name READ name CONSTANT)

public:
    TaskPortItem(QQuickItem *parent = nullptr);

    TaskPort *port() const;
    void setPort(TaskPort *newPort);

    QString name() const;

signals:
    void portChanged();

private:
    TaskPort *m_port = nullptr;
};

} // namespace QodeAssist::TaskFlow
