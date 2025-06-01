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
#include <QJsonArray>
#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QVariant>

#include "BaseTask.hpp"
#include "TaskConnection.hpp"
#include "TaskPort.hpp"

using namespace QodeAssist;

class MockTask : public BaseTask
{
    Q_OBJECT
public:
    explicit MockTask(const QString &taskId = "mock-task", QObject *parent = nullptr)
        : BaseTask(parent)
        , m_shouldFail(false)
        , m_executeCallCount(0)
    {
        setTaskId(taskId);
        addParameter("testParam", QString("defaultValue"));
        addParameter("numericParam", 42);
    }

    void setShouldFail(bool shouldFail) { m_shouldFail = shouldFail; }
    int executeCallCount() const { return m_executeCallCount; }

    TaskState execute() override
    {
        ++m_executeCallCount;

        if (m_shouldFail) {
            return TaskState::Failed;
        }

        return TaskState::Success;
    }

private:
    bool m_shouldFail;
    int m_executeCallCount;
};

class BaseTaskTest : public QObject, public testing::Test
{
    Q_OBJECT

protected:
    void SetUp() override { task = new MockTask("test-task"); }

    void TearDown() override { delete task; }

    MockTask *task;
};

TEST_F(BaseTaskTest, BasicProperties)
{
    EXPECT_EQ(task->taskId(), "test-task");

    task->setTaskId("new-task-id");
    EXPECT_EQ(task->taskId(), "new-task-id");
}

TEST_F(BaseTaskTest, EmptyTaskId)
{
    MockTask emptyTask("");
    EXPECT_EQ(emptyTask.taskId(), "");
    EXPECT_TRUE(emptyTask.taskId().isEmpty());
}

TEST_F(BaseTaskTest, ParameterManagement)
{
    EXPECT_EQ(task->getParameter("testParam").toString(), "defaultValue");
    EXPECT_EQ(task->getParameter("numericParam").toInt(), 42);

    task->setParameter("testParam", QString("newValue"));
    task->setParameter("numericParam", 100);

    EXPECT_EQ(task->getParameter("testParam").toString(), "newValue");
    EXPECT_EQ(task->getParameter("numericParam").toInt(), 100);

    task->addParameter("newParam", QString("added"));
    EXPECT_EQ(task->getParameter("newParam").toString(), "added");

    QVariant nonExisting = task->getParameter("nonExisting");
    EXPECT_FALSE(nonExisting.isValid());
}

TEST_F(BaseTaskTest, PortManagement)
{
    EXPECT_EQ(task->getInputPorts().size(), 0);
    EXPECT_EQ(task->getOutputPorts().size(), 0);

    task->addInputPort("input1");
    task->addInputPort("input2");

    EXPECT_EQ(task->getInputPorts().size(), 2);

    task->addOutputPort("output1");
    task->addOutputPort("output2");
    task->addOutputPort("output3");

    EXPECT_EQ(task->getOutputPorts().size(), 3);
}

TEST_F(BaseTaskTest, PortRetrieval)
{
    task->addInputPort("test_input");
    task->addOutputPort("test_output");

    TaskPort *inputPort = task->getInputPort("test_input");
    TaskPort *outputPort = task->getOutputPort("test_output");

    EXPECT_NE(inputPort, nullptr);
    EXPECT_NE(outputPort, nullptr);
    EXPECT_EQ(inputPort->name(), "test_input");
    EXPECT_EQ(outputPort->name(), "test_output");

    EXPECT_EQ(task->getInputPort("non_existing"), nullptr);
    EXPECT_EQ(task->getOutputPort("non_existing"), nullptr);
}

TEST_F(BaseTaskTest, DuplicatePortNames)
{
    task->addInputPort("same_name");
    task->addInputPort("same_name");

    EXPECT_EQ(task->getInputPorts().size(), 2);

    TaskPort *port = task->getInputPort("same_name");
    EXPECT_NE(port, nullptr);
    EXPECT_EQ(port->name(), "same_name");
}

TEST_F(BaseTaskTest, InputOutputValues)
{
    task->addInputPort("input_port");
    task->addOutputPort("output_port");

    QString testValue = "test_output_value";
    task->setOutputValue("output_port", testValue);

    TaskPort *outputPort = task->getOutputPort("output_port");
    EXPECT_EQ(outputPort->getValue().toString(), testValue);

    QVariant inputValue = task->getInputValue("input_port");
    EXPECT_FALSE(inputValue.isValid());

    task->setOutputValue("non_existing", "value");
}

TEST_F(BaseTaskTest, PortConnections)
{
    task->addInputPort("input");
    TaskPort *inputPort = task->getInputPort("input");

    TaskConnection *connection = new TaskConnection;

    task->setPortConnection(inputPort, connection);
    EXPECT_EQ(inputPort->connection(), connection);
    EXPECT_TRUE(inputPort->hasConnection());

    task->setPortConnection(nullptr, connection);

    delete connection;
}

TEST_F(BaseTaskTest, ExecuteSync)
{
    task->setShouldFail(false);
    TaskState result = task->execute();
    EXPECT_EQ(result, TaskState::Success);
    EXPECT_EQ(task->executeCallCount(), 1);

    task->setShouldFail(true);
    result = task->execute();
    EXPECT_EQ(result, TaskState::Failed);
    EXPECT_EQ(task->executeCallCount(), 2);
}

TEST_F(BaseTaskTest, ExecuteAsync)
{
    task->setShouldFail(false);

    QFuture<TaskState> future = task->executeAsync();

    future.waitForFinished();

    EXPECT_EQ(future.result(), TaskState::Success);
    EXPECT_EQ(task->executeCallCount(), 1);
}

TEST_F(BaseTaskTest, ExecuteAsyncFailed)
{
    task->setShouldFail(true);

    QFuture<TaskState> future = task->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), TaskState::Failed);
    EXPECT_EQ(task->executeCallCount(), 1);
}

TEST_F(BaseTaskTest, TaskStateToString)
{
    EXPECT_EQ(BaseTask::taskStateAsString(TaskState::Success), "Success");
    EXPECT_EQ(BaseTask::taskStateAsString(TaskState::Failed), "Failed");
    EXPECT_EQ(BaseTask::taskStateAsString(TaskState::Cancelled), "Cancelled");

    TaskState unknownState = static_cast<TaskState>(999);
    QString unknownString = BaseTask::taskStateAsString(unknownState);
    EXPECT_TRUE(unknownString.startsWith("Unknown("));
    EXPECT_TRUE(unknownString.contains("999"));
}

TEST_F(BaseTaskTest, ComplexDataTypes)
{
    task->addOutputPort("complex_output");

    QStringList stringList = {"item1", "item2", "item3"};
    task->setOutputValue("complex_output", stringList);

    TaskPort *port = task->getOutputPort("complex_output");
    QStringList retrieved = port->getValue().toStringList();
    EXPECT_EQ(retrieved.size(), 3);
    EXPECT_EQ(retrieved[0], "item1");

    QVariantMap map;
    map["key1"] = "value1";
    map["key2"] = 42;

    task->setOutputValue("complex_output", map);
    QVariantMap retrievedMap = port->getValue().toMap();
    EXPECT_EQ(retrievedMap.size(), 2);
    EXPECT_EQ(retrievedMap["key1"].toString(), "value1");
    EXPECT_EQ(retrievedMap["key2"].toInt(), 42);
}

TEST_F(BaseTaskTest, ThreadSafety)
{
    task->addInputPort("thread_input");
    task->addOutputPort("thread_output");

    QList<TaskPort *> inputs = task->getInputPorts();
    QList<TaskPort *> outputs = task->getOutputPorts();

    EXPECT_EQ(inputs.size(), 1);
    EXPECT_EQ(outputs.size(), 1);
}

TEST_F(BaseTaskTest, MultipleAsyncExecutions)
{
    task->setShouldFail(false);

    QFuture<TaskState> future1 = task->executeAsync();
    QFuture<TaskState> future2 = task->executeAsync();
    QFuture<TaskState> future3 = task->executeAsync();

    future1.waitForFinished();
    future2.waitForFinished();
    future3.waitForFinished();

    EXPECT_EQ(future1.result(), TaskState::Success);
    EXPECT_EQ(future2.result(), TaskState::Success);
    EXPECT_EQ(future3.result(), TaskState::Success);
    EXPECT_EQ(task->executeCallCount(), 3);
}

TEST_F(BaseTaskTest, LargeNumberOfPorts)
{
    const int portCount = 100;

    for (int i = 0; i < portCount; ++i) {
        task->addInputPort(QString("input_%1").arg(i));
        task->addOutputPort(QString("output_%1").arg(i));
    }

    EXPECT_EQ(task->getInputPorts().size(), portCount);
    EXPECT_EQ(task->getOutputPorts().size(), portCount);

    TaskPort *input50 = task->getInputPort("input_50");
    TaskPort *output75 = task->getOutputPort("output_75");

    EXPECT_NE(input50, nullptr);
    EXPECT_NE(output75, nullptr);
    EXPECT_EQ(input50->name(), "input_50");
    EXPECT_EQ(output75->name(), "output_75");
}

TEST_F(BaseTaskTest, PortValueTypes)
{
    task->addOutputPort("type_test");

    task->setOutputValue("type_test", 42);
    EXPECT_EQ(task->getOutputPort("type_test")->getValue().toInt(), 42);

    task->setOutputValue("type_test", 3.14159);
    EXPECT_DOUBLE_EQ(task->getOutputPort("type_test")->getValue().toDouble(), 3.14159);

    task->setOutputValue("type_test", true);
    EXPECT_EQ(task->getOutputPort("type_test")->getValue().toBool(), true);

    task->setOutputValue("type_test", QString("test string"));
    EXPECT_EQ(task->getOutputPort("type_test")->getValue().toString(), "test string");
}

TEST_F(BaseTaskTest, JsonSerialization)
{
    task->setTaskId("json-test-task");
    task->addInputPort("input1");
    task->addInputPort("input2");
    task->addOutputPort("output1");
    task->addOutputPort("output2");

    task->setParameter("testParam", QString("serialized_value"));
    task->setParameter("numericParam", 123);

    QJsonObject json = task->toJson();

    EXPECT_EQ(json["taskId"].toString(), "json-test-task");
    EXPECT_EQ(json["taskType"].toString(), "MockTask");
    EXPECT_TRUE(json.contains("params"));

    QJsonObject params = json["params"].toObject();
    EXPECT_FALSE(params.isEmpty());
    EXPECT_EQ(params["testParam"].toString(), "serialized_value");
    EXPECT_EQ(params["numericParam"].toInt(), 123);
}

TEST_F(BaseTaskTest, JsonDeserialization)
{
    QJsonObject json;
    json["taskId"] = "deserialized-task";
    json["taskType"] = "MockTask";

    QJsonObject params;
    params["testParam"] = "deserialized_value";
    params["numericParam"] = 999;
    params["newParam"] = "added_value";
    json["params"] = params;

    bool result = task->fromJson(json);
    EXPECT_TRUE(result);
    EXPECT_EQ(task->taskId(), "deserialized-task");

    EXPECT_EQ(task->getParameter("testParam").toString(), "deserialized_value");
    EXPECT_EQ(task->getParameter("numericParam").toInt(), 999);
    EXPECT_EQ(task->getParameter("newParam").toString(), "added_value");
}

TEST_F(BaseTaskTest, JsonDeserializationMissingTaskId)
{
    QJsonObject json;
    json["taskType"] = "MockTask";

    QJsonObject params;
    params["testParam"] = "value";
    json["params"] = params;

    bool result = task->fromJson(json);
    EXPECT_FALSE(result);
}

TEST_F(BaseTaskTest, JsonRoundTrip)
{
    task->setTaskId("roundtrip-task");
    task->setParameter("testParam", QString("roundtrip_value"));
    task->setParameter("numericParam", 777);

    QJsonObject originalJson = task->toJson();

    MockTask newTask;
    bool result = newTask.fromJson(originalJson);
    EXPECT_TRUE(result);
    EXPECT_EQ(newTask.taskId(), "roundtrip-task");

    EXPECT_EQ(newTask.getParameter("testParam").toString(), "roundtrip_value");
    EXPECT_EQ(newTask.getParameter("numericParam").toInt(), 777);

    QJsonObject newJson = newTask.toJson();
    EXPECT_EQ(originalJson["taskId"], newJson["taskId"]);
    EXPECT_EQ(originalJson["taskType"], newJson["taskType"]);

    QJsonObject originalParams = originalJson["params"].toObject();
    QJsonObject newParams = newJson["params"].toObject();
    EXPECT_EQ(originalParams["testParam"], newParams["testParam"]);
    EXPECT_EQ(originalParams["numericParam"], newParams["numericParam"]);
}

TEST_F(BaseTaskTest, GetTaskType)
{
    QString taskType = task->getTaskType();
    EXPECT_EQ(taskType, "MockTask");
}

TEST_F(BaseTaskTest, EmptyJsonObject)
{
    QJsonObject emptyJson;
    bool result = task->fromJson(emptyJson);
    EXPECT_FALSE(result);
}

TEST_F(BaseTaskTest, JsonWithComplexParams)
{
    task->setParameter("stringParam", QString("test_string"));
    task->setParameter("intParam", 42);
    task->setParameter("doubleParam", 3.14159);
    task->setParameter("boolParam", true);

    QJsonObject json = task->toJson();
    QJsonObject params = json["params"].toObject();

    EXPECT_FALSE(params.isEmpty());
    EXPECT_EQ(params.size(), 6);
    EXPECT_EQ(params["stringParam"].toString(), "test_string");
    EXPECT_EQ(params["intParam"].toInt(), 42);
    EXPECT_DOUBLE_EQ(params["doubleParam"].toDouble(), 3.14159);
    EXPECT_EQ(params["boolParam"].toBool(), true);
}

TEST_F(BaseTaskTest, TaskIdValidation)
{
    task->setTaskId("simple-id");
    EXPECT_EQ(task->taskId(), "simple-id");

    task->setTaskId("123-numeric-start");
    EXPECT_EQ(task->taskId(), "123-numeric-start");

    task->setTaskId("with_underscores");
    EXPECT_EQ(task->taskId(), "with_underscores");

    task->setTaskId("with.dots");
    EXPECT_EQ(task->taskId(), "with.dots");
}

TEST_F(BaseTaskTest, GetConnectedValueFromPort)
{
    task->addInputPort("connected_input");
    TaskPort *inputPort = task->getInputPort("connected_input");

    QVariant value = task->getInputValue("connected_input");
    EXPECT_FALSE(value.isValid());

    MockTask sourceTask("source-task");
    sourceTask.addOutputPort("source_output");
    sourceTask.setOutputValue("source_output", "connected_value");

    TaskPort *sourcePort = sourceTask.getOutputPort("source_output");

    TaskConnection connection;
    connection.sourceTask = &sourceTask;
    connection.targetTask = task;
    connection.sourcePort = sourcePort;
    connection.targetPort = inputPort;

    inputPort->setConnection(&connection);

    value = task->getInputValue("connected_input");
    EXPECT_TRUE(value.isValid());
    EXPECT_EQ(value.toString(), "connected_value");
}

TEST_F(BaseTaskTest, ParameterOverride)
{
    task->addParameter("existingParam", QString("original"));

    task->addParameter("existingParam", QString("new_default"));
    EXPECT_EQ(task->getParameter("existingParam").toString(), "original");

    task->setParameter("existingParam", QString("updated"));
    EXPECT_EQ(task->getParameter("existingParam").toString(), "updated");
}

TEST_F(BaseTaskTest, ParameterTypes)
{
    task->addParameter("stringParam", QString("default_string"));
    task->addParameter("intParam", 100);
    task->addParameter("doubleParam", 2.718);
    task->addParameter("boolParam", false);

    EXPECT_EQ(task->getParameter("stringParam").type(), QVariant::String);
    EXPECT_EQ(task->getParameter("intParam").type(), QVariant::Int);
    EXPECT_EQ(task->getParameter("doubleParam").type(), QVariant::Double);
    EXPECT_EQ(task->getParameter("boolParam").type(), QVariant::Bool);
}

TEST_F(BaseTaskTest, JsonWithEmptyParams)
{
    MockTask emptyParamTask("empty-params");

    QJsonObject json = emptyParamTask.toJson();
    EXPECT_TRUE(json.contains("params"));

    QJsonObject params = json["params"].toObject();
    EXPECT_FALSE(params.isEmpty());
}

#include "BaseTaskTest.moc"
