/*
 * Copyright (C) 2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https:
 */

#include <gtest/gtest.h>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include "BaseTask.hpp"
#include "TaskConnection.hpp"
#include "TaskPort.hpp"

using namespace QodeAssist;

class MockTaskForConnection : public BaseTask
{
    Q_OBJECT

public:
    explicit MockTaskForConnection(const QString &taskId = "mock-task", QObject *parent = nullptr)
        : BaseTask(parent)
    {
        setTaskId(taskId);
    }

    TaskState execute() override { return TaskState::Success; }
};

class TaskConnectionTest : public testing::Test
{
protected:
    void SetUp() override
    {
        sourceTask = new MockTaskForConnection("source-task");
        targetTask = new MockTaskForConnection("target-task");

        sourceTask->addOutputPort("output1");
        sourceTask->addOutputPort("output2");

        targetTask->addInputPort("input1");
        targetTask->addInputPort("input2");

        sourcePort = sourceTask->getOutputPort("output1");
        targetPort = targetTask->getInputPort("input1");
    }

    void TearDown() override
    {
        delete sourceTask;
        delete targetTask;
    }

    MockTaskForConnection *sourceTask;
    MockTaskForConnection *targetTask;
    TaskPort *sourcePort;
    TaskPort *targetPort;
};

TEST_F(TaskConnectionTest, BasicConnectionCreation)
{
    TaskConnection connection;
    connection.sourceTask = sourceTask;
    connection.targetTask = targetTask;
    connection.sourcePort = sourcePort;
    connection.targetPort = targetPort;

    EXPECT_EQ(connection.sourceTask, sourceTask);
    EXPECT_EQ(connection.targetTask, targetTask);
    EXPECT_EQ(connection.sourcePort, sourcePort);
    EXPECT_EQ(connection.targetPort, targetPort);
}

TEST_F(TaskConnectionTest, EqualityOperator)
{
    TaskConnection connection1;
    connection1.sourceTask = sourceTask;
    connection1.targetTask = targetTask;
    connection1.sourcePort = sourcePort;
    connection1.targetPort = targetPort;

    TaskConnection connection2;
    connection2.sourceTask = sourceTask;
    connection2.targetTask = targetTask;
    connection2.sourcePort = sourcePort;
    connection2.targetPort = targetPort;

    EXPECT_TRUE(connection1 == connection2);

    connection2.sourceTask = nullptr;
    EXPECT_FALSE(connection1 == connection2);
}

TEST_F(TaskConnectionTest, ToStringValid)
{
    TaskConnection connection;
    connection.sourceTask = sourceTask;
    connection.targetTask = targetTask;
    connection.sourcePort = sourcePort;
    connection.targetPort = targetPort;

    QString connectionStr = connection.toString();
    EXPECT_EQ(connectionStr, "source-task.output1->target-task.input1");
}

TEST_F(TaskConnectionTest, ToStringWithNullFields)
{
    TaskConnection connection;

    connection.sourceTask = nullptr;
    connection.targetTask = targetTask;
    connection.sourcePort = sourcePort;
    connection.targetPort = targetPort;

    QString connectionStr = connection.toString();
    EXPECT_TRUE(connectionStr.isEmpty());

    connection.sourceTask = sourceTask;
    connection.targetTask = nullptr;
    connectionStr = connection.toString();
    EXPECT_TRUE(connectionStr.isEmpty());

    connection.targetTask = targetTask;
    connection.sourcePort = nullptr;
    connectionStr = connection.toString();
    EXPECT_TRUE(connectionStr.isEmpty());

    connection.sourcePort = sourcePort;
    connection.targetPort = nullptr;
    connectionStr = connection.toString();
    EXPECT_TRUE(connectionStr.isEmpty());
}

TEST_F(TaskConnectionTest, FromStringValid)
{
    QHash<QString, BaseTask *> tasks;
    tasks["source-task"] = sourceTask;
    tasks["target-task"] = targetTask;

    QString connectionStr = "source-task.output1->target-task.input1";
    TaskConnection *connection = TaskConnection::fromString(connectionStr, tasks);

    EXPECT_NE(connection, nullptr);
    EXPECT_EQ(connection->sourceTask, sourceTask);
    EXPECT_EQ(connection->targetTask, targetTask);
    EXPECT_EQ(connection->sourcePort, sourcePort);
    EXPECT_EQ(connection->targetPort, targetPort);

    delete connection;
}

TEST_F(TaskConnectionTest, FromStringInvalidFormat)
{
    QHash<QString, BaseTask *> tasks;
    tasks["source-task"] = sourceTask;
    tasks["target-task"] = targetTask;

    EXPECT_EQ(TaskConnection::fromString("", tasks), nullptr);
    EXPECT_EQ(TaskConnection::fromString("invalid-format", tasks), nullptr);
    EXPECT_EQ(TaskConnection::fromString("source-task->target-task", tasks), nullptr);
    EXPECT_EQ(
        TaskConnection::fromString("source-task.port1.extra->target-task.port2", tasks), nullptr);
    EXPECT_EQ(
        TaskConnection::fromString("source-task.port1->target-task.port2.extra", tasks), nullptr);
}

TEST_F(TaskConnectionTest, FromStringMissingTasks)
{
    QHash<QString, BaseTask *> tasks;
    tasks["source-task"] = sourceTask;

    QString connectionStr = "source-task.output1->target-task.input1";
    TaskConnection *connection = TaskConnection::fromString(connectionStr, tasks);

    EXPECT_EQ(connection, nullptr);
}

TEST_F(TaskConnectionTest, FromStringMissingPorts)
{
    QHash<QString, BaseTask *> tasks;
    tasks["source-task"] = sourceTask;
    tasks["target-task"] = targetTask;

    QString connectionStr = "source-task.non_existing->target-task.input1";
    TaskConnection *connection = TaskConnection::fromString(connectionStr, tasks);
    EXPECT_EQ(connection, nullptr);

    connectionStr = "source-task.output1->target-task.non_existing";
    connection = TaskConnection::fromString(connectionStr, tasks);
    EXPECT_EQ(connection, nullptr);
}

TEST_F(TaskConnectionTest, ToJsonValid)
{
    TaskConnection connection;
    connection.sourceTask = sourceTask;
    connection.targetTask = targetTask;
    connection.sourcePort = sourcePort;
    connection.targetPort = targetPort;

    QJsonObject json = connection.toJson();

    EXPECT_EQ(json["sourceTask"].toString(), "source-task");
    EXPECT_EQ(json["sourcePort"].toString(), "output1");
    EXPECT_EQ(json["targetTask"].toString(), "target-task");
    EXPECT_EQ(json["targetPort"].toString(), "input1");
    EXPECT_EQ(json["connectionString"].toString(), "source-task.output1->target-task.input1");
}

TEST_F(TaskConnectionTest, ToJsonWithNullFields)
{
    TaskConnection connection;

    QJsonObject json = connection.toJson();

    EXPECT_TRUE(json.isEmpty());
    EXPECT_FALSE(json.contains("sourceTask"));
    EXPECT_FALSE(json.contains("sourcePort"));
    EXPECT_FALSE(json.contains("targetTask"));
    EXPECT_FALSE(json.contains("targetPort"));
    EXPECT_FALSE(json.contains("connectionString"));
}

TEST_F(TaskConnectionTest, FromJsonValid)
{
    QHash<QString, BaseTask *> tasks;
    tasks["source-task"] = sourceTask;
    tasks["target-task"] = targetTask;

    QJsonObject json;
    json["sourceTask"] = "source-task";
    json["sourcePort"] = "output1";
    json["targetTask"] = "target-task";
    json["targetPort"] = "input1";

    TaskConnection *connection = TaskConnection::fromJson(json, tasks);

    EXPECT_NE(connection, nullptr);
    EXPECT_EQ(connection->sourceTask, sourceTask);
    EXPECT_EQ(connection->targetTask, targetTask);
    EXPECT_EQ(connection->sourcePort, sourcePort);
    EXPECT_EQ(connection->targetPort, targetPort);

    delete connection;
}

TEST_F(TaskConnectionTest, FromJsonMissingTasks)
{
    QHash<QString, BaseTask *> tasks;
    tasks["source-task"] = sourceTask;

    QJsonObject json;
    json["sourceTask"] = "source-task";
    json["sourcePort"] = "output1";
    json["targetTask"] = "target-task";
    json["targetPort"] = "input1";

    TaskConnection *connection = TaskConnection::fromJson(json, tasks);
    EXPECT_EQ(connection, nullptr);
}

TEST_F(TaskConnectionTest, FromJsonMissingPorts)
{
    QHash<QString, BaseTask *> tasks;
    tasks["source-task"] = sourceTask;
    tasks["target-task"] = targetTask;

    QJsonObject json;
    json["sourceTask"] = "source-task";
    json["sourcePort"] = "non_existing";
    json["targetTask"] = "target-task";
    json["targetPort"] = "input1";

    TaskConnection *connection = TaskConnection::fromJson(json, tasks);
    EXPECT_EQ(connection, nullptr);
}

TEST_F(TaskConnectionTest, RoundTripStringSerialization)
{
    QHash<QString, BaseTask *> tasks;
    tasks["source-task"] = sourceTask;
    tasks["target-task"] = targetTask;

    TaskConnection original;
    original.sourceTask = sourceTask;
    original.targetTask = targetTask;
    original.sourcePort = sourcePort;
    original.targetPort = targetPort;

    QString connectionStr = original.toString();
    TaskConnection *restored = TaskConnection::fromString(connectionStr, tasks);

    EXPECT_NE(restored, nullptr);
    EXPECT_TRUE(original == *restored);

    delete restored;
}

TEST_F(TaskConnectionTest, RoundTripJsonSerialization)
{
    QHash<QString, BaseTask *> tasks;
    tasks["source-task"] = sourceTask;
    tasks["target-task"] = targetTask;

    TaskConnection original;
    original.sourceTask = sourceTask;
    original.targetTask = targetTask;
    original.sourcePort = sourcePort;
    original.targetPort = targetPort;

    QJsonObject json = original.toJson();
    TaskConnection *restored = TaskConnection::fromJson(json, tasks);

    EXPECT_NE(restored, nullptr);
    EXPECT_TRUE(original == *restored);

    delete restored;
}

TEST_F(TaskConnectionTest, ConnectionWithDifferentPortTypes)
{
    TaskPort *sourcePort2 = sourceTask->getOutputPort("output2");
    TaskPort *targetPort2 = targetTask->getInputPort("input2");

    TaskConnection connection;
    connection.sourceTask = sourceTask;
    connection.targetTask = targetTask;
    connection.sourcePort = sourcePort2;
    connection.targetPort = targetPort2;

    QString connectionStr = connection.toString();
    EXPECT_EQ(connectionStr, "source-task.output2->target-task.input2");
}

TEST_F(TaskConnectionTest, EmptyTasksHash)
{
    QHash<QString, BaseTask *> emptyTasks;

    QString connectionStr = "source-task.output1->target-task.input1";
    TaskConnection *connection = TaskConnection::fromString(connectionStr, emptyTasks);

    EXPECT_EQ(connection, nullptr);
}

TEST_F(TaskConnectionTest, ConnectionStringEdgeCases)
{
    QHash<QString, BaseTask *> tasks;
    tasks["source-task"] = sourceTask;
    tasks["target-task"] = targetTask;

    EXPECT_EQ(TaskConnection::fromString("source->middle->target", tasks), nullptr);

    EXPECT_EQ(TaskConnection::fromString("source.port1 target.port2", tasks), nullptr);

    EXPECT_EQ(TaskConnection::fromString("source.port.extra->target.port", tasks), nullptr);
}

#include "TaskConnectionTest.moc"
