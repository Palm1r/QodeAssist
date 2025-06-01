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
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariant>

#include "BaseTask.hpp"
#include "TaskConnection.hpp"
#include "TaskPort.hpp"

using namespace QodeAssist;

class MockSourceTask : public BaseTask
{
public:
    explicit MockSourceTask(QObject *parent = nullptr)
        : BaseTask(parent)
    {
        setTaskId("mock-source");
        addOutputPort("output");
    }

protected:
    TaskState execute() override
    {
        setOutputValue("output", QString("test_data"));
        return TaskState::Success;
    }
};

class MockTargetTask : public BaseTask
{
public:
    explicit MockTargetTask(QObject *parent = nullptr)
        : BaseTask(parent)
    {
        setTaskId("mock-target");
        addInputPort("input");
    }

protected:
    TaskState execute() override
    {
        QVariant value = getInputValue("input");
        m_receivedValue = value;
        return TaskState::Success;
    }

public:
    QVariant m_receivedValue;
};

class TaskPortTest : public QObject, public testing::Test
{
    Q_OBJECT

protected:
    void SetUp() override
    {
        sourceTask = new MockSourceTask();
        targetTask = new MockTargetTask();

        sourcePort = sourceTask->getOutputPort("output");
        targetPort = targetTask->getInputPort("input");

        connection = nullptr;
    }

    void TearDown() override
    {
        delete sourceTask;
        delete targetTask;
        delete connection;
    }

    MockSourceTask *sourceTask;
    MockTargetTask *targetTask;
    TaskPort *sourcePort;
    TaskPort *targetPort;
    TaskConnection *connection;
};

TEST_F(TaskPortTest, BasicProperties)
{
    TaskPort port("test_port");

    EXPECT_EQ(port.name(), "test_port");
    EXPECT_FALSE(port.hasConnection());
    EXPECT_EQ(port.connection(), nullptr);
}

TEST_F(TaskPortTest, ValueStorage)
{
    TaskPort port("value_port");

    QString testString = "Hello World";
    port.setValue(testString);
    EXPECT_EQ(port.getValue().toString(), testString);

    int testInt = 42;
    port.setValue(testInt);
    EXPECT_EQ(port.getValue().toInt(), testInt);

    double testDouble = 3.14159;
    port.setValue(testDouble);
    EXPECT_DOUBLE_EQ(port.getValue().toDouble(), testDouble);

    bool testBool = true;
    port.setValue(testBool);
    EXPECT_EQ(port.getValue().toBool(), testBool);

    QDateTime testDateTime = QDateTime::currentDateTime();
    port.setValue(testDateTime);
    EXPECT_EQ(port.getValue().toDateTime(), testDateTime);
}

TEST_F(TaskPortTest, InvalidValue)
{
    TaskPort port("invalid_port");

    EXPECT_FALSE(port.getValue().isValid());

    port.setValue(QVariant());
    EXPECT_FALSE(port.getValue().isValid());

    port.setValue(42);
    EXPECT_TRUE(port.getValue().isValid());

    port.setValue(QVariant());
    EXPECT_FALSE(port.getValue().isValid());
}

TEST_F(TaskPortTest, ConnectionSetup)
{
    ASSERT_NE(sourcePort, nullptr);
    ASSERT_NE(targetPort, nullptr);

    connection = new TaskConnection;
    connection->sourceTask = sourceTask;
    connection->targetTask = targetTask;
    connection->sourcePort = sourcePort;
    connection->targetPort = targetPort;

    targetPort->setConnection(connection);

    EXPECT_TRUE(targetPort->hasConnection());
    EXPECT_EQ(targetPort->connection(), connection);

    sourcePort->setConnection(connection);
    EXPECT_TRUE(sourcePort->hasConnection());
    EXPECT_EQ(sourcePort->connection(), connection);
}

TEST_F(TaskPortTest, ConnectedValueRetrieval)
{
    ASSERT_NE(sourcePort, nullptr);
    ASSERT_NE(targetPort, nullptr);

    QString testValue = "connected_data";
    sourcePort->setValue(testValue);

    connection = new TaskConnection;
    connection->sourceTask = sourceTask;
    connection->targetTask = targetTask;
    connection->sourcePort = sourcePort;
    connection->targetPort = targetPort;

    targetPort->setConnection(connection);

    QVariant connectedValue = targetPort->getConnectedValue();
    EXPECT_EQ(connectedValue.toString(), testValue);
}

TEST_F(TaskPortTest, ConnectedValueWithoutConnection)
{
    TaskPort port("unconnected_port");

    QVariant value = port.getConnectedValue();
    EXPECT_FALSE(value.isValid());
}

TEST_F(TaskPortTest, ConnectedValueWithNullSourcePort)
{
    connection = new TaskConnection;
    connection->sourceTask = sourceTask;
    connection->targetTask = targetTask;
    connection->sourcePort = nullptr;
    connection->targetPort = targetPort;

    targetPort->setConnection(connection);

    QVariant value = targetPort->getConnectedValue();
    EXPECT_FALSE(value.isValid());
}

TEST_F(TaskPortTest, LocalVsConnectedValue)
{
    ASSERT_NE(sourcePort, nullptr);
    ASSERT_NE(targetPort, nullptr);

    QString localValue = "local_data";
    targetPort->setValue(localValue);

    QString connectedValue = "connected_data";
    sourcePort->setValue(connectedValue);

    connection = new TaskConnection;
    connection->sourceTask = sourceTask;
    connection->targetTask = targetTask;
    connection->sourcePort = sourcePort;
    connection->targetPort = targetPort;

    targetPort->setConnection(connection);

    EXPECT_EQ(targetPort->getValue().toString(), localValue);
    EXPECT_EQ(targetPort->getConnectedValue().toString(), connectedValue);
    EXPECT_NE(targetPort->getValue(), targetPort->getConnectedValue());
}

TEST_F(TaskPortTest, ValueTypeConversions)
{
    TaskPort port("conversion_port");

    port.setValue(123);
    EXPECT_EQ(port.getValue().toString(), "123");

    port.setValue(QString("456"));
    EXPECT_EQ(port.getValue().toInt(), 456);

    port.setValue(true);
    EXPECT_EQ(port.getValue().toString(), "true");

    port.setValue(QString("false"));
    EXPECT_EQ(port.getValue().toBool(), false);
}

TEST_F(TaskPortTest, ConnectionReplacement)
{
    TaskPort port("replacement_port");

    TaskConnection *connection1 = new TaskConnection;
    port.setConnection(connection1);
    EXPECT_EQ(port.connection(), connection1);

    TaskConnection *connection2 = new TaskConnection;
    port.setConnection(connection2);
    EXPECT_EQ(port.connection(), connection2);
    EXPECT_NE(port.connection(), connection1);

    delete connection1;
    delete connection2;
}

TEST_F(TaskPortTest, NullConnectionHandling)
{
    TaskPort port("null_port");

    port.setConnection(nullptr);
    EXPECT_FALSE(port.hasConnection());
    EXPECT_EQ(port.connection(), nullptr);

    QVariant value = port.getConnectedValue();
    EXPECT_FALSE(value.isValid());
}

TEST_F(TaskPortTest, EmptyPortName)
{
    TaskPort port("");

    EXPECT_EQ(port.name(), "");
    EXPECT_TRUE(port.name().isEmpty());
}

TEST_F(TaskPortTest, LargeDataHandling)
{
    TaskPort port("large_data_port");

    QString largeString(10000, 'A');
    port.setValue(largeString);
    EXPECT_EQ(port.getValue().toString().length(), 10000);
    EXPECT_EQ(port.getValue().toString(), largeString);

    QByteArray largeData(50000, 'B');
    port.setValue(largeData);
    EXPECT_EQ(port.getValue().toByteArray().size(), 50000);
    EXPECT_EQ(port.getValue().toByteArray(), largeData);
}

TEST_F(TaskPortTest, ComplexDataTypes)
{
    TaskPort port("complex_port");

    QStringList stringList = {"item1", "item2", "item3"};
    port.setValue(stringList);
    QStringList retrievedList = port.getValue().toStringList();
    EXPECT_EQ(retrievedList.size(), 3);
    EXPECT_EQ(retrievedList[0], "item1");
    EXPECT_EQ(retrievedList[2], "item3");

    QVariantMap map;
    map["key1"] = "value1";
    map["key2"] = 42;
    map["key3"] = true;

    port.setValue(map);
    QVariantMap retrievedMap = port.getValue().toMap();
    EXPECT_EQ(retrievedMap.size(), 3);
    EXPECT_EQ(retrievedMap["key1"].toString(), "value1");
    EXPECT_EQ(retrievedMap["key2"].toInt(), 42);
    EXPECT_EQ(retrievedMap["key3"].toBool(), true);
}

TEST_F(TaskPortTest, MultipleConnectedValues)
{
    MockSourceTask *sourceTask2 = new MockSourceTask();
    TaskPort *sourcePort2 = sourceTask2->getOutputPort("output");

    sourcePort->setValue(QString("value1"));
    sourcePort2->setValue(QString("value2"));

    connection = new TaskConnection;
    connection->sourceTask = sourceTask;
    connection->targetTask = targetTask;
    connection->sourcePort = sourcePort;
    connection->targetPort = targetPort;

    targetPort->setConnection(connection);
    EXPECT_EQ(targetPort->getConnectedValue().toString(), "value1");

    TaskConnection *connection2 = new TaskConnection;
    connection2->sourceTask = sourceTask2;
    connection2->targetTask = targetTask;
    connection2->sourcePort = sourcePort2;
    connection2->targetPort = targetPort;

    targetPort->setConnection(connection2);
    EXPECT_EQ(targetPort->getConnectedValue().toString(), "value2");

    delete sourceTask2;
    delete connection2;
}

TEST_F(TaskPortTest, ValuePersistenceAfterConnectionChange)
{
    TaskPort port("persistence_port");

    QString localValue = "persistent_value";
    port.setValue(localValue);

    connection = new TaskConnection;
    port.setConnection(connection);

    EXPECT_EQ(port.getValue().toString(), localValue);

    port.setConnection(nullptr);

    EXPECT_EQ(port.getValue().toString(), localValue);
}

#include "TaskPortTest.moc"
