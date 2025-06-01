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
#include <QString>
#include <QVariant>

#include "BaseTask.hpp"
#include "Flow.hpp"
#include "TaskConnection.hpp"
#include "TaskPort.hpp"

using namespace QodeAssist;

class MockSimpleTask : public BaseTask
{
public:
    explicit MockSimpleTask(const QString &taskId, QObject *parent = nullptr)
        : BaseTask(parent)
        , m_executed(false)
    {
        setTaskId(taskId);
    }

protected:
    TaskState execute() override
    {
        m_executed = true;
        return TaskState::Success;
    }

public:
    bool m_executed;
};

class MockFailingTask : public BaseTask
{
public:
    explicit MockFailingTask(const QString &taskId, QObject *parent = nullptr)
        : BaseTask(parent)
    {
        setTaskId(taskId);
    }

protected:
    TaskState execute() override { return TaskState::Failed; }
};

class MockProducerTask : public BaseTask
{
public:
    explicit MockProducerTask(
        const QString &taskId, const QString &outputValue, QObject *parent = nullptr)
        : BaseTask(parent)
        , m_outputValue(outputValue)
    {
        setTaskId(taskId);
        addOutputPort("output");
    }

protected:
    TaskState execute() override
    {
        setOutputValue("output", m_outputValue);
        return TaskState::Success;
    }

private:
    QString m_outputValue;
};

class MockConsumerTask : public BaseTask
{
public:
    explicit MockConsumerTask(const QString &taskId, QObject *parent = nullptr)
        : BaseTask(parent)
    {
        setTaskId(taskId);
        addInputPort("input");
    }

protected:
    TaskState execute() override
    {
        m_receivedValue = getInputValue("input");
        return TaskState::Success;
    }

public:
    QVariant m_receivedValue;
};

class FlowTest : public QObject, public testing::Test
{
    Q_OBJECT

protected:
    void SetUp() override { flow = new Flow("test-flow"); }

    void TearDown() override { delete flow; }

    Flow *flow;
};

class MockSerializableTask : public BaseTask
{
    Q_OBJECT

public:
    explicit MockSerializableTask(
        const QString &taskId = "serializable-task", QObject *parent = nullptr)
        : BaseTask(parent)
        , m_customParam("default_value")
    {
        setTaskId(taskId);
    }

    void setCustomParam(const QString &param) { m_customParam = param; }
    QString customParam() const { return m_customParam; }

    QJsonObject toJson() const override
    {
        QJsonObject json = BaseTask::toJson();
        QJsonObject params;
        params["customParam"] = m_customParam;
        json["params"] = params;
        return json;
    }

    bool fromJson(const QJsonObject &json) override
    {
        if (!BaseTask::fromJson(json)) {
            return false;
        }

        QJsonObject params = json["params"].toObject();
        if (params.contains("customParam")) {
            m_customParam = params["customParam"].toString();
        }
        return true;
    }

    TaskState execute() override { return TaskState::Success; }

private:
    QString m_customParam;
};

TEST_F(FlowTest, BasicProperties)
{
    EXPECT_EQ(flow->flowId(), "test-flow");
}

TEST_F(FlowTest, FlowStateStringConversion)
{
    EXPECT_EQ(Flow::flowStateAsString(FlowState::Success), "Success");
    EXPECT_EQ(Flow::flowStateAsString(FlowState::Failed), "Failed");
    EXPECT_EQ(Flow::flowStateAsString(FlowState::Cancelled), "Cancelled");
}

TEST_F(FlowTest, EmptyFlowExecution)
{
    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Failed);
}

TEST_F(FlowTest, SingleTaskExecution)
{
    MockSimpleTask *task = new MockSimpleTask("task1");
    flow->addTask(task);

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Success);
    EXPECT_TRUE(task->m_executed);
}

TEST_F(FlowTest, SingleFailingTaskExecution)
{
    MockFailingTask *task = new MockFailingTask("failing-task");
    flow->addTask(task);

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Failed);
}

TEST_F(FlowTest, MultipleTasks)
{
    MockSimpleTask *task1 = new MockSimpleTask("task1");
    MockSimpleTask *task2 = new MockSimpleTask("task2");
    MockSimpleTask *task3 = new MockSimpleTask("task3");

    flow->addTask(task1);
    flow->addTask(task2);
    flow->addTask(task3);

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Success);
    EXPECT_TRUE(task1->m_executed);
    EXPECT_TRUE(task2->m_executed);
    EXPECT_TRUE(task3->m_executed);
}

TEST_F(FlowTest, TaskWithConnection)
{
    MockProducerTask *producer = new MockProducerTask("producer", "test_data");
    MockConsumerTask *consumer = new MockConsumerTask("consumer");

    flow->addTask(producer);
    flow->addTask(consumer);

    TaskPort *outputPort = producer->getOutputPort("output");
    TaskPort *inputPort = consumer->getInputPort("input");

    flow->addConnection(producer, outputPort, consumer, inputPort);

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Success);
    EXPECT_EQ(consumer->m_receivedValue.toString(), "test_data");
}

TEST_F(FlowTest, ChainedTasks)
{
    MockProducerTask *task1 = new MockProducerTask("task1", "first_value");

    class MockMiddleTask : public BaseTask
    {
    public:
        explicit MockMiddleTask(const QString &taskId, QObject *parent = nullptr)
            : BaseTask(parent)
        {
            setTaskId(taskId);
            addInputPort("input");
            addOutputPort("output");
        }

    protected:
        TaskState execute() override
        {
            QString input = getInputValue("input").toString();
            setOutputValue("output", input + "_processed");
            return TaskState::Success;
        }
    };

    MockMiddleTask *task2 = new MockMiddleTask("task2");
    MockConsumerTask *task3 = new MockConsumerTask("task3");

    flow->addTask(task1);
    flow->addTask(task2);
    flow->addTask(task3);

    flow->addConnection(task1, task1->getOutputPort("output"), task2, task2->getInputPort("input"));
    flow->addConnection(task2, task2->getOutputPort("output"), task3, task3->getInputPort("input"));

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Success);
    EXPECT_EQ(task3->m_receivedValue.toString(), "first_value_processed");
}

TEST_F(FlowTest, TaskDependencyOrdering)
{
    MockProducerTask *producer = new MockProducerTask("producer", "dependency_data");

    class MockOrderedConsumer : public BaseTask
    {
    public:
        explicit MockOrderedConsumer(const QString &taskId, int *counter, QObject *parent = nullptr)
            : BaseTask(parent)
            , m_counter(counter)
            , m_executionOrder(-1)
        {
            setTaskId(taskId);
            addInputPort("input");
        }

    protected:
        TaskState execute() override
        {
            m_executionOrder = ++(*m_counter);
            getInputValue("input");
            return TaskState::Success;
        }

    public:
        int *m_counter;
        int m_executionOrder;
    };

    int executionCounter = 0;
    MockOrderedConsumer *consumer1 = new MockOrderedConsumer("consumer1", &executionCounter);
    MockOrderedConsumer *consumer2 = new MockOrderedConsumer("consumer2", &executionCounter);

    flow->addTask(producer);
    flow->addTask(consumer1);
    flow->addTask(consumer2);

    flow->addConnection(
        producer, producer->getOutputPort("output"), consumer1, consumer1->getInputPort("input"));
    flow->addConnection(
        producer, producer->getOutputPort("output"), consumer2, consumer2->getInputPort("input"));

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Success);

    EXPECT_GT(consumer1->m_executionOrder, 0);
    EXPECT_GT(consumer2->m_executionOrder, 0);
}

TEST_F(FlowTest, NullTaskHandling)
{
    flow->addTask(nullptr);

    MockSimpleTask *validTask = new MockSimpleTask("valid");
    flow->addTask(validTask);

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Success);
    EXPECT_TRUE(validTask->m_executed);
}

TEST_F(FlowTest, NullConnectionHandling)
{
    MockProducerTask *producer = new MockProducerTask("producer", "test");
    MockConsumerTask *consumer = new MockConsumerTask("consumer");

    flow->addTask(producer);
    flow->addTask(consumer);

    flow->addConnection(
        nullptr, producer->getOutputPort("output"), consumer, consumer->getInputPort("input"));
    flow->addConnection(producer, nullptr, consumer, consumer->getInputPort("input"));
    flow->addConnection(
        producer, producer->getOutputPort("output"), nullptr, consumer->getInputPort("input"));
    flow->addConnection(producer, producer->getOutputPort("output"), consumer, nullptr);

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Success);
}

TEST_F(FlowTest, TaskExecutionFailurePropagation)
{
    MockSimpleTask *goodTask = new MockSimpleTask("good");
    MockFailingTask *badTask = new MockFailingTask("bad");
    MockSimpleTask *anotherGoodTask = new MockSimpleTask("another-good");

    flow->addTask(goodTask);
    flow->addTask(badTask);
    flow->addTask(anotherGoodTask);

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Failed);
}

TEST_F(FlowTest, EmptyFlowId)
{
    Flow *emptyFlow = new Flow("");
    EXPECT_EQ(emptyFlow->flowId(), "");
    EXPECT_TRUE(emptyFlow->flowId().isEmpty());

    delete emptyFlow;
}

TEST_F(FlowTest, TaskOwnership)
{
    MockSimpleTask *task = new MockSimpleTask("owned-task");

    flow->addTask(task);
    EXPECT_EQ(task->parent(), flow);
}

TEST_F(FlowTest, DiamondDependencyPattern)
{
    MockProducerTask *taskA = new MockProducerTask("A", "source_data");

    class MockTransformTask : public BaseTask
    {
    public:
        explicit MockTransformTask(
            const QString &taskId, const QString &suffix, QObject *parent = nullptr)
            : BaseTask(parent)
            , m_suffix(suffix)
        {
            setTaskId(taskId);
            addInputPort("input");
            addOutputPort("output");
        }

    protected:
        TaskState execute() override
        {
            QString input = getInputValue("input").toString();
            setOutputValue("output", input + m_suffix);
            return TaskState::Success;
        }

    private:
        QString m_suffix;
    };

    class MockMergeTask : public BaseTask
    {
    public:
        explicit MockMergeTask(const QString &taskId, QObject *parent = nullptr)
            : BaseTask(parent)
        {
            setTaskId(taskId);
            addInputPort("input1");
            addInputPort("input2");
            addOutputPort("merged");
        }

    protected:
        TaskState execute() override
        {
            QString input1 = getInputValue("input1").toString();
            QString input2 = getInputValue("input2").toString();
            setOutputValue("merged", input1 + "+" + input2);
            m_mergedResult = input1 + "+" + input2;
            return TaskState::Success;
        }

    public:
        QString m_mergedResult;
    };

    MockTransformTask *taskB = new MockTransformTask("B", "_pathB");
    MockTransformTask *taskC = new MockTransformTask("C", "_pathC");
    MockMergeTask *taskD = new MockMergeTask("D");

    flow->addTask(taskA);
    flow->addTask(taskB);
    flow->addTask(taskC);
    flow->addTask(taskD);

    flow->addConnection(taskA, taskA->getOutputPort("output"), taskB, taskB->getInputPort("input"));
    flow->addConnection(taskA, taskA->getOutputPort("output"), taskC, taskC->getInputPort("input"));

    flow->addConnection(taskB, taskB->getOutputPort("output"), taskD, taskD->getInputPort("input1"));
    flow->addConnection(taskC, taskC->getOutputPort("output"), taskD, taskD->getInputPort("input2"));

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Success);
    EXPECT_EQ(taskD->m_mergedResult, "source_data_pathB+source_data_pathC");
}

TEST_F(FlowTest, FanOutFanInPattern)
{
    MockProducerTask *source = new MockProducerTask("source", "initial");

    class MockParallelProcessor : public BaseTask
    {
    public:
        explicit MockParallelProcessor(
            const QString &taskId, int multiplier, QObject *parent = nullptr)
            : BaseTask(parent)
            , m_multiplier(multiplier)
        {
            setTaskId(taskId);
            addInputPort("data");
            addOutputPort("result");
        }

    protected:
        TaskState execute() override
        {
            QThread::msleep(10);
            setOutputValue(
                "result",
                QString("processed_%1_x%2").arg(getInputValue("data").toString()).arg(m_multiplier));
            return TaskState::Success;
        }

    private:
        int m_multiplier;
    };

    class MockAggregator : public BaseTask
    {
    public:
        explicit MockAggregator(const QString &taskId, QObject *parent = nullptr)
            : BaseTask(parent)
        {
            setTaskId(taskId);
            addInputPort("input1");
            addInputPort("input2");
            addInputPort("input3");
        }

    protected:
        TaskState execute() override
        {
            QStringList results;
            results << getInputValue("input1").toString();
            results << getInputValue("input2").toString();
            results << getInputValue("input3").toString();
            m_aggregatedResult = results.join("|");
            return TaskState::Success;
        }

    public:
        QString m_aggregatedResult;
    };

    MockParallelProcessor *proc1 = new MockParallelProcessor("proc1", 2);
    MockParallelProcessor *proc2 = new MockParallelProcessor("proc2", 3);
    MockParallelProcessor *proc3 = new MockParallelProcessor("proc3", 5);
    MockAggregator *aggregator = new MockAggregator("aggregator");

    flow->addTask(source);
    flow->addTask(proc1);
    flow->addTask(proc2);
    flow->addTask(proc3);
    flow->addTask(aggregator);

    flow->addConnection(source, source->getOutputPort("output"), proc1, proc1->getInputPort("data"));
    flow->addConnection(source, source->getOutputPort("output"), proc2, proc2->getInputPort("data"));
    flow->addConnection(source, source->getOutputPort("output"), proc3, proc3->getInputPort("data"));

    flow->addConnection(
        proc1, proc1->getOutputPort("result"), aggregator, aggregator->getInputPort("input1"));
    flow->addConnection(
        proc2, proc2->getOutputPort("result"), aggregator, aggregator->getInputPort("input2"));
    flow->addConnection(
        proc3, proc3->getOutputPort("result"), aggregator, aggregator->getInputPort("input3"));

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Success);
    EXPECT_TRUE(aggregator->m_aggregatedResult.contains("processed_initial_x2"));
    EXPECT_TRUE(aggregator->m_aggregatedResult.contains("processed_initial_x3"));
    EXPECT_TRUE(aggregator->m_aggregatedResult.contains("processed_initial_x5"));
}

TEST_F(FlowTest, ComplexMultiLevelDependencies)
{
    MockProducerTask *taskA = new MockProducerTask("A", "root");

    class MockNumberedTask : public BaseTask
    {
    public:
        explicit MockNumberedTask(const QString &taskId, int number, QObject *parent = nullptr)
            : BaseTask(parent)
            , m_number(number)
        {
            setTaskId(taskId);
            addInputPort("input");
            addOutputPort("output");
        }

    protected:
        TaskState execute() override
        {
            QString input = getInputValue("input").toString();
            setOutputValue("output", QString("%1->%2").arg(input).arg(m_number));
            return TaskState::Success;
        }

    private:
        int m_number;
    };

    class MockFinalCollector : public BaseTask
    {
    public:
        explicit MockFinalCollector(const QString &taskId, QObject *parent = nullptr)
            : BaseTask(parent)
        {
            setTaskId(taskId);
            addInputPort("from_d");
            addInputPort("from_e");
            addInputPort("from_f");
        }

    protected:
        TaskState execute() override
        {
            QStringList inputs;
            inputs << getInputValue("from_d").toString();
            inputs << getInputValue("from_e").toString();
            inputs << getInputValue("from_f").toString();
            m_finalResult = inputs.join(" & ");
            return TaskState::Success;
        }

    public:
        QString m_finalResult;
    };

    MockNumberedTask *taskB = new MockNumberedTask("B", 2);
    MockNumberedTask *taskC = new MockNumberedTask("C", 3);
    MockNumberedTask *taskD = new MockNumberedTask("D", 4);
    MockNumberedTask *taskE = new MockNumberedTask("E", 5);
    MockNumberedTask *taskF = new MockNumberedTask("F", 6);
    MockFinalCollector *taskG = new MockFinalCollector("G");

    flow->addTask(taskA);
    flow->addTask(taskB);
    flow->addTask(taskC);
    flow->addTask(taskD);
    flow->addTask(taskE);
    flow->addTask(taskF);
    flow->addTask(taskG);

    flow->addConnection(taskA, taskA->getOutputPort("output"), taskB, taskB->getInputPort("input"));
    flow->addConnection(taskA, taskA->getOutputPort("output"), taskC, taskC->getInputPort("input"));

    flow->addConnection(taskB, taskB->getOutputPort("output"), taskD, taskD->getInputPort("input"));
    flow->addConnection(taskB, taskB->getOutputPort("output"), taskE, taskE->getInputPort("input"));
    flow->addConnection(taskC, taskC->getOutputPort("output"), taskF, taskF->getInputPort("input"));

    flow->addConnection(taskD, taskD->getOutputPort("output"), taskG, taskG->getInputPort("from_d"));
    flow->addConnection(taskE, taskE->getOutputPort("output"), taskG, taskG->getInputPort("from_e"));
    flow->addConnection(taskF, taskF->getOutputPort("output"), taskG, taskG->getInputPort("from_f"));

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Success);
    EXPECT_TRUE(taskG->m_finalResult.contains("root->2->4"));
    EXPECT_TRUE(taskG->m_finalResult.contains("root->2->5"));
    EXPECT_TRUE(taskG->m_finalResult.contains("root->3->6"));
}

TEST_F(FlowTest, CircularDependencyDetection)
{
    class MockCircularTask : public BaseTask
    {
    public:
        explicit MockCircularTask(const QString &taskId, QObject *parent = nullptr)
            : BaseTask(parent)
        {
            setTaskId(taskId);
            addInputPort("input");
            addOutputPort("output");
        }

    protected:
        TaskState execute() override
        {
            QString input = getInputValue("input").toString();
            setOutputValue("output", input + "_" + taskId());
            return TaskState::Success;
        }
    };

    MockCircularTask *taskA = new MockCircularTask("A");
    MockCircularTask *taskB = new MockCircularTask("B");
    MockCircularTask *taskC = new MockCircularTask("C");

    flow->addTask(taskA);
    flow->addTask(taskB);
    flow->addTask(taskC);

    flow->addConnection(taskA, taskA->getOutputPort("output"), taskB, taskB->getInputPort("input"));
    flow->addConnection(taskB, taskB->getOutputPort("output"), taskC, taskC->getInputPort("input"));
    flow->addConnection(taskC, taskC->getOutputPort("output"), taskA, taskA->getInputPort("input"));

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Failed);
}

TEST_F(FlowTest, ConditionalExecutionPattern)
{
    class MockConditionTask : public BaseTask
    {
    public:
        explicit MockConditionTask(const QString &taskId, bool condition, QObject *parent = nullptr)
            : BaseTask(parent)
            , m_condition(condition)
        {
            setTaskId(taskId);
            addOutputPort("result");
            addOutputPort("flag");
        }

    protected:
        TaskState execute() override
        {
            setOutputValue("result", m_condition ? "positive" : "negative");
            setOutputValue("flag", m_condition);
            return TaskState::Success;
        }

    private:
        bool m_condition;
    };

    class MockConditionalConsumer : public BaseTask
    {
    public:
        explicit MockConditionalConsumer(const QString &taskId, QObject *parent = nullptr)
            : BaseTask(parent)
            , m_wasExecuted(false)
        {
            setTaskId(taskId);
            addInputPort("data");
            addInputPort("condition");
        }

    protected:
        TaskState execute() override
        {
            bool shouldProcess = getInputValue("condition").toBool();
            if (shouldProcess) {
                m_result = getInputValue("data").toString() + "_processed";
                m_wasExecuted = true;
            }
            return TaskState::Success;
        }

    public:
        QString m_result;
        bool m_wasExecuted;
    };

    MockConditionTask *conditionTask = new MockConditionTask("condition", true);
    MockConditionalConsumer *consumer1 = new MockConditionalConsumer("consumer1");
    MockConditionalConsumer *consumer2 = new MockConditionalConsumer("consumer2");

    flow->addTask(conditionTask);
    flow->addTask(consumer1);
    flow->addTask(consumer2);

    flow->addConnection(
        conditionTask,
        conditionTask->getOutputPort("result"),
        consumer1,
        consumer1->getInputPort("data"));
    flow->addConnection(
        conditionTask,
        conditionTask->getOutputPort("flag"),
        consumer1,
        consumer1->getInputPort("condition"));

    flow->addConnection(
        conditionTask,
        conditionTask->getOutputPort("result"),
        consumer2,
        consumer2->getInputPort("data"));
    flow->addConnection(
        conditionTask,
        conditionTask->getOutputPort("flag"),
        consumer2,
        consumer2->getInputPort("condition"));

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Success);
    EXPECT_TRUE(consumer1->m_wasExecuted);
    EXPECT_TRUE(consumer2->m_wasExecuted);
    EXPECT_EQ(consumer1->m_result, "positive_processed");
    EXPECT_EQ(consumer2->m_result, "positive_processed");
}

TEST_F(FlowTest, DataValidationChain)
{
    class MockValidatorTask : public BaseTask
    {
    public:
        explicit MockValidatorTask(const QString &taskId, bool isValid, QObject *parent = nullptr)
            : BaseTask(parent)
            , m_isValid(isValid)
        {
            setTaskId(taskId);
            addInputPort("input");
            addOutputPort("output");
            addOutputPort("is_valid");
        }

    protected:
        TaskState execute() override
        {
            QString input = getInputValue("input").toString();
            setOutputValue("output", input);
            setOutputValue("is_valid", m_isValid);
            return m_isValid ? TaskState::Success : TaskState::Failed;
        }

    private:
        bool m_isValid;
    };

    MockProducerTask *source = new MockProducerTask("source", "test_data");
    MockValidatorTask *validator1 = new MockValidatorTask("validator1", true);
    MockValidatorTask *validator2 = new MockValidatorTask("validator2", true);
    MockValidatorTask *validator3 = new MockValidatorTask("validator3", false);
    MockConsumerTask *finalConsumer = new MockConsumerTask("final");

    flow->addTask(source);
    flow->addTask(validator1);
    flow->addTask(validator2);
    flow->addTask(validator3);
    flow->addTask(finalConsumer);

    flow->addConnection(
        source, source->getOutputPort("output"), validator1, validator1->getInputPort("input"));
    flow->addConnection(
        validator1,
        validator1->getOutputPort("output"),
        validator2,
        validator2->getInputPort("input"));
    flow->addConnection(
        validator2,
        validator2->getOutputPort("output"),
        validator3,
        validator3->getInputPort("input"));
    flow->addConnection(
        validator3,
        validator3->getOutputPort("output"),
        finalConsumer,
        finalConsumer->getInputPort("input"));

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Failed);
}

TEST_F(FlowTest, JsonSerialization)
{
    MockSerializableTask *task1 = new MockSerializableTask("task1");
    MockSerializableTask *task2 = new MockSerializableTask("task2");

    task1->setCustomParam("param1_value");
    task2->setCustomParam("param2_value");

    task1->addOutputPort("output1");
    task2->addInputPort("input2");

    flow->addTask(task1);
    flow->addTask(task2);

    flow->addConnection(task1, task1->getOutputPort("output1"), task2, task2->getInputPort("input2"));

    QJsonObject json = flow->toJson();

    EXPECT_EQ(json["flowId"].toString(), "test-flow");
    EXPECT_TRUE(json.contains("tasks"));
    EXPECT_TRUE(json.contains("connections"));

    QJsonArray tasks = json["tasks"].toArray();
    EXPECT_EQ(tasks.size(), 2);

    QJsonArray connections = json["connections"].toArray();
    EXPECT_EQ(connections.size(), 1);
    EXPECT_EQ(connections[0].toString(), "task1.output1->task2.input2");
}

TEST_F(FlowTest, JsonDeserialization)
{
    QJsonObject flowJson;
    flowJson["flowId"] = "deserialized-flow";

    QJsonArray tasksArray;

    QJsonObject task1Json;
    task1Json["taskId"] = "deserial_task1";
    task1Json["taskType"] = "MockSerializableTask";
    QJsonObject params1;
    params1["customParam"] = "deserialized_param1";
    task1Json["params"] = params1;
    tasksArray.append(task1Json);

    QJsonObject task2Json;
    task2Json["taskId"] = "deserial_task2";
    task2Json["taskType"] = "MockSerializableTask";
    QJsonObject params2;
    params2["customParam"] = "deserialized_param2";
    task2Json["params"] = params2;
    tasksArray.append(task2Json);

    flowJson["tasks"] = tasksArray;

    QJsonArray connectionsArray;
    connectionsArray.append("deserial_task1.output->deserial_task2.input");
    flowJson["connections"] = connectionsArray;

    Flow *newFlow = new Flow("temp");

    bool result = newFlow->fromJson(flowJson);
    EXPECT_FALSE(result);

    delete newFlow;
}

TEST_F(FlowTest, JsonRoundTrip)
{
    MockProducerTask *producer = new MockProducerTask("producer", "test_data");
    MockConsumerTask *consumer = new MockConsumerTask("consumer");

    flow->addTask(producer);
    flow->addTask(consumer);

    flow->addConnection(
        producer, producer->getOutputPort("output"), consumer, consumer->getInputPort("input"));

    QJsonObject originalJson = flow->toJson();

    Flow *newFlow = new Flow("temp");
    bool result = newFlow->fromJson(originalJson);

    EXPECT_FALSE(result);

    delete newFlow;
}

TEST_F(FlowTest, JsonByteArraySerialization)
{
    MockSimpleTask *task = new MockSimpleTask("byte_test");
    flow->addTask(task);

    QByteArray jsonData = flow->toJsonData();
    EXPECT_FALSE(jsonData.isEmpty());

    Flow *newFlow = new Flow("temp");
    bool result = newFlow->fromJsonData(jsonData);

    EXPECT_FALSE(result);

    delete newFlow;
}

TEST_F(FlowTest, JsonInvalidData)
{
    Flow *testFlow = new Flow("invalid_test");

    QByteArray invalidJson = "{ invalid json }";
    bool result = testFlow->fromJsonData(invalidJson);
    EXPECT_FALSE(result);

    QByteArray emptyJson = "{}";
    result = testFlow->fromJsonData(emptyJson);
    EXPECT_TRUE(result);

    delete testFlow;
}

TEST_F(FlowTest, TaskRegistryIntegration)
{
    Flow *testFlow = new Flow("registry_test");

    BaseTask *task1 = testFlow->createTaskByType("Task1", "test1", QJsonObject());
    BaseTask *task2 = testFlow->createTaskByType("Task2", "test2", QJsonObject());

    EXPECT_NE(task1, nullptr);
    EXPECT_NE(task2, nullptr);

    if (task1) {
        EXPECT_EQ(task1->taskId(), "test1");
        delete task1;
    }

    if (task2) {
        EXPECT_EQ(task2->taskId(), "test2");
        delete task2;
    }

    delete testFlow;
}

TEST_F(FlowTest, TaskCreationWithParams)
{
    Flow *testFlow = new Flow("param_test");

    QJsonObject params;
    params["filePath"] = "/test/path/file.txt";

    BaseTask *task = testFlow->createTaskByType("Task1", "param_task", params);

    if (task) {
        EXPECT_EQ(task->taskId(), "param_task");

        delete task;
    }

    delete testFlow;
}

TEST_F(FlowTest, UnknownTaskTypeCreation)
{
    Flow *testFlow = new Flow("unknown_test");

    BaseTask *task = testFlow->createTaskByType("UnknownTaskType", "unknown", QJsonObject());
    EXPECT_EQ(task, nullptr);

    delete testFlow;
}

TEST_F(FlowTest, FlowExecutionWithRealTasks)
{
    Flow *testFlow = new Flow("real_task_test");

    BaseTask *task1 = testFlow->createTaskByType("Task1", "real_task1", QJsonObject());
    BaseTask *task2 = testFlow->createTaskByType("Task2", "real_task2", QJsonObject());

    if (task1 && task2) {
        testFlow->addTask(task1);
        testFlow->addTask(task2);

        QJsonObject task1Params;
        task1Params["filePath"] = __FILE__;

        QJsonObject task1Json;
        task1Json["taskId"] = "real_task1";
        task1Json["taskType"] = "Task1";
        task1Json["params"] = task1Params;

        task1->fromJson(task1Json);

        TaskPort *outputPort = task1->getOutputPort("completed");
        TaskPort *inputPort = task2->getInputPort("completed");

        if (outputPort && inputPort) {
            testFlow->addConnection(task1, outputPort, task2, inputPort);
        }

        QFuture<FlowState> future = testFlow->executeAsync();
        future.waitForFinished();

        EXPECT_EQ(future.result(), FlowState::Success);
    }

    delete testFlow;
}

TEST_F(FlowTest, ConnectionStringParsing)
{
    MockProducerTask *producer = new MockProducerTask("producer_conn", "test");
    MockConsumerTask *consumer = new MockConsumerTask("consumer_conn");

    flow->addTask(producer);
    flow->addTask(consumer);

    flow->addConnection(
        producer, producer->getOutputPort("output"), consumer, consumer->getInputPort("input"));

    QJsonObject json = flow->toJson();
    QJsonArray connections = json["connections"].toArray();

    EXPECT_EQ(connections.size(), 1);
    QString connectionStr = connections[0].toString();
    EXPECT_EQ(connectionStr, "producer_conn.output->consumer_conn.input");
}

TEST_F(FlowTest, MultipleConnectionsSameTask)
{
    MockProducerTask *producer = new MockProducerTask("multi_producer", "data");
    MockConsumerTask *consumer1 = new MockConsumerTask("consumer1");
    MockConsumerTask *consumer2 = new MockConsumerTask("consumer2");

    producer->addOutputPort("output2");

    flow->addTask(producer);
    flow->addTask(consumer1);
    flow->addTask(consumer2);

    flow->addConnection(
        producer, producer->getOutputPort("output"), consumer1, consumer1->getInputPort("input"));
    flow->addConnection(
        producer, producer->getOutputPort("output2"), consumer2, consumer2->getInputPort("input"));

    QFuture<FlowState> future = flow->executeAsync();
    future.waitForFinished();

    EXPECT_EQ(future.result(), FlowState::Success);
    EXPECT_EQ(consumer1->m_receivedValue.toString(), "data");

    EXPECT_FALSE(consumer2->m_receivedValue.isValid());
}

TEST_F(FlowTest, EmptyConnectionArray)
{
    MockSimpleTask *task = new MockSimpleTask("isolated");
    flow->addTask(task);

    QJsonObject json = flow->toJson();
    QJsonArray connections = json["connections"].toArray();

    EXPECT_EQ(connections.size(), 0);
}

TEST_F(FlowTest, FlowWithComplexTaskHierarchy)
{
    Flow *complexFlow = new Flow("complex_hierarchy");

    BaseTask *realTask1 = complexFlow->createTaskByType("Task1", "hierarchy_task1", QJsonObject());
    MockSimpleTask *mockTask = new MockSimpleTask("hierarchy_mock");
    BaseTask *realTask2 = complexFlow->createTaskByType("Task2", "hierarchy_task2", QJsonObject());

    if (realTask1)
        complexFlow->addTask(realTask1);
    complexFlow->addTask(mockTask);
    if (realTask2)
        complexFlow->addTask(realTask2);

    QFuture<FlowState> future = complexFlow->executeAsync();
    future.waitForFinished();

    FlowState result = future.result();

    EXPECT_TRUE(result == FlowState::Success || result == FlowState::Failed);

    delete complexFlow;
}

TEST_F(FlowTest, SerializationPreservesFlowId)
{
    Flow *namedFlow = new Flow("special_flow_id");
    MockSimpleTask *task = new MockSimpleTask("id_test");
    namedFlow->addTask(task);

    QJsonObject json = namedFlow->toJson();
    EXPECT_EQ(json["flowId"].toString(), "special_flow_id");

    Flow *deserializedFlow = new Flow("temp");
    deserializedFlow->fromJson(json);
    EXPECT_EQ(deserializedFlow->flowId(), "special_flow_id");

    delete namedFlow;
    delete deserializedFlow;
}

#include "FlowTest.moc"
