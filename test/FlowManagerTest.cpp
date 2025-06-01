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
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTemporaryFile>

#include "BaseTask.hpp"
#include "DemoTasks.hpp"
#include "Flow.hpp"
#include "FlowManager.hpp"

using namespace QodeAssist;

class MockTaskForFlowManager : public BaseTask
{
    Q_OBJECT

public:
    explicit MockTaskForFlowManager(
        const QString &taskId = "mock-flow-task", QObject *parent = nullptr)
        : BaseTask(parent)
    {
        setTaskId(taskId);
    }

    TaskState execute() override { return TaskState::Success; }
};

class FlowManagerTest : public QObject, public testing::Test
{
    Q_OBJECT

protected:
    void SetUp() override { manager = new FlowManager(); }

    void TearDown() override { delete manager; }

    FlowManager *manager;
};

TEST_F(FlowManagerTest, BasicProperties)
{
    EXPECT_EQ(manager->getAllFlows().size(), 0);
    EXPECT_TRUE(manager->getFlowIds().isEmpty());
    EXPECT_FALSE(manager->hasFlow("non-existent"));
}

TEST_F(FlowManagerTest, AddSingleFlow)
{
    Flow *flow = new Flow("test-flow-1");
    manager->addFlow(flow);

    EXPECT_EQ(manager->getAllFlows().size(), 1);
    EXPECT_TRUE(manager->hasFlow("test-flow-1"));
    EXPECT_EQ(manager->getFlow("test-flow-1"), flow);
    EXPECT_TRUE(manager->getFlowIds().contains("test-flow-1"));
}

TEST_F(FlowManagerTest, AddMultipleFlows)
{
    Flow *flow1 = new Flow("flow-1");
    Flow *flow2 = new Flow("flow-2");
    Flow *flow3 = new Flow("flow-3");

    manager->addFlow(flow1);
    manager->addFlow(flow2);
    manager->addFlow(flow3);

    EXPECT_EQ(manager->getAllFlows().size(), 3);
    EXPECT_TRUE(manager->hasFlow("flow-1"));
    EXPECT_TRUE(manager->hasFlow("flow-2"));
    EXPECT_TRUE(manager->hasFlow("flow-3"));

    QStringList flowIds = manager->getFlowIds();
    EXPECT_TRUE(flowIds.contains("flow-1"));
    EXPECT_TRUE(flowIds.contains("flow-2"));
    EXPECT_TRUE(flowIds.contains("flow-3"));
}

TEST_F(FlowManagerTest, AddNullFlow)
{
    manager->addFlow(nullptr);

    EXPECT_EQ(manager->getAllFlows().size(), 0);
    EXPECT_TRUE(manager->getFlowIds().isEmpty());
}

TEST_F(FlowManagerTest, AddFlowWithEmptyId)
{
    Flow *flow = new Flow("");
    manager->addFlow(flow);

    EXPECT_EQ(manager->getAllFlows().size(), 0);
    EXPECT_TRUE(manager->getFlowIds().isEmpty());

    delete flow;
}

TEST_F(FlowManagerTest, ReplaceExistingFlow)
{
    Flow *originalFlow = new Flow("same-id");
    Flow *replacementFlow = new Flow("same-id");

    manager->addFlow(originalFlow);
    EXPECT_EQ(manager->getAllFlows().size(), 1);
    EXPECT_EQ(manager->getFlow("same-id"), originalFlow);

    manager->addFlow(replacementFlow);
    EXPECT_EQ(manager->getAllFlows().size(), 1);
    EXPECT_EQ(manager->getFlow("same-id"), replacementFlow);
    EXPECT_NE(manager->getFlow("same-id"), originalFlow);
}

TEST_F(FlowManagerTest, RemoveExistingFlow)
{
    Flow *flow = new Flow("removable-flow");
    manager->addFlow(flow);

    EXPECT_TRUE(manager->hasFlow("removable-flow"));
    EXPECT_EQ(manager->getAllFlows().size(), 1);

    manager->removeFlow("removable-flow");

    EXPECT_FALSE(manager->hasFlow("removable-flow"));
    EXPECT_EQ(manager->getAllFlows().size(), 0);
    EXPECT_EQ(manager->getFlow("removable-flow"), nullptr);
}

TEST_F(FlowManagerTest, RemoveNonExistentFlow)
{
    manager->removeFlow("non-existent");
    EXPECT_EQ(manager->getAllFlows().size(), 0);
}

TEST_F(FlowManagerTest, GetNonExistentFlow)
{
    EXPECT_EQ(manager->getFlow("non-existent"), nullptr);
}

TEST_F(FlowManagerTest, ClearFlows)
{
    Flow *flow1 = new Flow("flow-1");
    Flow *flow2 = new Flow("flow-2");
    Flow *flow3 = new Flow("flow-3");

    manager->addFlow(flow1);
    manager->addFlow(flow2);
    manager->addFlow(flow3);

    EXPECT_EQ(manager->getAllFlows().size(), 3);

    manager->clear();

    EXPECT_EQ(manager->getAllFlows().size(), 0);
    EXPECT_TRUE(manager->getFlowIds().isEmpty());
    EXPECT_FALSE(manager->hasFlow("flow-1"));
    EXPECT_FALSE(manager->hasFlow("flow-2"));
    EXPECT_FALSE(manager->hasFlow("flow-3"));
}

TEST_F(FlowManagerTest, FlowOwnership)
{
    Flow *flow = new Flow("owned-flow");

    manager->addFlow(flow);
    EXPECT_EQ(flow->parent(), manager);
}

TEST_F(FlowManagerTest, JsonSerialization)
{
    Flow *flow1 = new Flow("json-flow-1");
    Flow *flow2 = new Flow("json-flow-2");

    MockTaskForFlowManager *task1 = new MockTaskForFlowManager("task1");
    MockTaskForFlowManager *task2 = new MockTaskForFlowManager("task2");

    flow1->addTask(task1);
    flow2->addTask(task2);

    manager->addFlow(flow1);
    manager->addFlow(flow2);

    QJsonObject json = manager->toJson();

    EXPECT_TRUE(json.contains("flows"));
    EXPECT_TRUE(json.contains("flowCount"));
    EXPECT_EQ(json["flowCount"].toInt(), 2);

    QJsonArray flows = json["flows"].toArray();
    EXPECT_EQ(flows.size(), 2);
}

TEST_F(FlowManagerTest, JsonDeserialization)
{
    QJsonObject managerJson;

    QJsonArray flowsArray;

    QJsonObject flow1Json;
    flow1Json["flowId"] = "deserialized-flow-1";
    QJsonArray tasks1;
    QJsonObject task1Json;
    task1Json["taskId"] = "task1";
    task1Json["taskType"] = "Task1";
    tasks1.append(task1Json);
    flow1Json["tasks"] = tasks1;
    flow1Json["connections"] = QJsonArray();
    flowsArray.append(flow1Json);

    QJsonObject flow2Json;
    flow2Json["flowId"] = "deserialized-flow-2";
    QJsonArray tasks2;
    QJsonObject task2Json;
    task2Json["taskId"] = "task2";
    task2Json["taskType"] = "Task2";
    tasks2.append(task2Json);
    flow2Json["tasks"] = tasks2;
    flow2Json["connections"] = QJsonArray();
    flowsArray.append(flow2Json);

    managerJson["flows"] = flowsArray;
    managerJson["flowCount"] = 2;

    bool result = manager->fromJson(managerJson);
    EXPECT_TRUE(result);
    EXPECT_EQ(manager->getAllFlows().size(), 2);
    EXPECT_TRUE(manager->hasFlow("deserialized-flow-1"));
    EXPECT_TRUE(manager->hasFlow("deserialized-flow-2"));
}

TEST_F(FlowManagerTest, JsonDeserializationEmptyFlows)
{
    QJsonObject emptyJson;
    emptyJson["flows"] = QJsonArray();
    emptyJson["flowCount"] = 0;

    bool result = manager->fromJson(emptyJson);
    EXPECT_TRUE(result);
    EXPECT_EQ(manager->getAllFlows().size(), 0);
}

TEST_F(FlowManagerTest, JsonDeserializationInvalidJson)
{
    QJsonObject invalidJson;
    invalidJson["invalid"] = "data";

    bool result = manager->fromJson(invalidJson);
    EXPECT_FALSE(result);
    EXPECT_EQ(manager->getAllFlows().size(), 0);
}

TEST_F(FlowManagerTest, JsonRoundTrip)
{
    Flow *originalFlow = new Flow("roundtrip-flow");

    BaseTask *task = originalFlow->createTaskByType("Task1", "roundtrip-task", QJsonObject());
    if (task) {
        originalFlow->addTask(task);
    }

    manager->addFlow(originalFlow);

    QJsonObject originalJson = manager->toJson();

    FlowManager newManager;
    bool result = newManager.fromJson(originalJson);
    EXPECT_TRUE(result);

    if (task) {
        EXPECT_EQ(newManager.getAllFlows().size(), 1);
        EXPECT_TRUE(newManager.hasFlow("roundtrip-flow"));

        QJsonObject newJson = newManager.toJson();
        EXPECT_EQ(originalJson["flowCount"], newJson["flowCount"]);
    } else {
        EXPECT_EQ(newManager.getAllFlows().size(), 1);
        EXPECT_TRUE(newManager.hasFlow("roundtrip-flow"));
    }
}

TEST_F(FlowManagerTest, SaveToFile)
{
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    EXPECT_TRUE(tempFile.open());
    QString filePath = tempFile.fileName();
    tempFile.close();

    Flow *flow1 = new Flow("save-flow-1");
    Flow *flow2 = new Flow("save-flow-2");
    manager->addFlow(flow1);
    manager->addFlow(flow2);

    bool result = manager->saveToFile(filePath);
    EXPECT_TRUE(result);

    QFileInfo fileInfo(filePath);
    EXPECT_TRUE(fileInfo.exists());
    EXPECT_GT(fileInfo.size(), 0);

    QFile::remove(filePath);
}

TEST_F(FlowManagerTest, SaveToInvalidPath)
{
    Flow *flow = new Flow("test-flow");
    manager->addFlow(flow);

    bool result = manager->saveToFile("/invalid/path/file.json");
    EXPECT_FALSE(result);
}

TEST_F(FlowManagerTest, SaveEmptyPath)
{
    bool result = manager->saveToFile("");
    EXPECT_FALSE(result);
}

TEST_F(FlowManagerTest, LoadFromFile)
{
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    EXPECT_TRUE(tempFile.open());

    QJsonObject managerJson;
    QJsonArray flowsArray;

    QJsonObject flowJson;
    flowJson["flowId"] = "loaded-flow";
    QJsonArray tasks;
    QJsonObject taskJson;
    taskJson["taskId"] = "loaded-task";
    taskJson["taskType"] = "Task1";
    tasks.append(taskJson);
    flowJson["tasks"] = tasks;
    flowJson["connections"] = QJsonArray();
    flowsArray.append(flowJson);

    managerJson["flows"] = flowsArray;
    managerJson["flowCount"] = 1;

    QJsonDocument doc(managerJson);
    tempFile.write(doc.toJson());
    QString filePath = tempFile.fileName();
    tempFile.close();

    bool result = manager->loadFromFile(filePath);
    EXPECT_TRUE(result);
    EXPECT_EQ(manager->getAllFlows().size(), 1);
    EXPECT_TRUE(manager->hasFlow("loaded-flow"));

    QFile::remove(filePath);
}

TEST_F(FlowManagerTest, LoadFromNonExistentFile)
{
    bool result = manager->loadFromFile("/non/existent/file.json");
    EXPECT_FALSE(result);
    EXPECT_EQ(manager->getAllFlows().size(), 0);
}

TEST_F(FlowManagerTest, LoadFromInvalidJson)
{
    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    EXPECT_TRUE(tempFile.open());
    tempFile.write("{ invalid json content }");
    QString filePath = tempFile.fileName();
    tempFile.close();

    bool result = manager->loadFromFile(filePath);
    EXPECT_FALSE(result);
    EXPECT_EQ(manager->getAllFlows().size(), 0);

    QFile::remove(filePath);
}

TEST_F(FlowManagerTest, LoadEmptyPath)
{
    bool result = manager->loadFromFile("");
    EXPECT_FALSE(result);
    EXPECT_EQ(manager->getAllFlows().size(), 0);
}

TEST_F(FlowManagerTest, SaveLoadRoundTrip)
{
    Flow *originalFlow = new Flow("roundtrip-file-flow");

    BaseTask *task = originalFlow->createTaskByType("Task1", "file-task", QJsonObject());
    if (task) {
        originalFlow->addTask(task);
    }

    manager->addFlow(originalFlow);

    QTemporaryFile tempFile;
    tempFile.setAutoRemove(false);
    EXPECT_TRUE(tempFile.open());
    QString filePath = tempFile.fileName();
    tempFile.close();

    bool saveResult = manager->saveToFile(filePath);
    EXPECT_TRUE(saveResult);

    FlowManager newManager;
    bool loadResult = newManager.loadFromFile(filePath);
    EXPECT_TRUE(loadResult);

    if (task) {
        EXPECT_EQ(newManager.getAllFlows().size(), 1);
        EXPECT_TRUE(newManager.hasFlow("roundtrip-file-flow"));
    } else {
        EXPECT_EQ(newManager.getAllFlows().size(), 1);
        EXPECT_TRUE(newManager.hasFlow("roundtrip-file-flow"));
    }

    QFile::remove(filePath);
}

TEST_F(FlowManagerTest, LargeNumberOfFlows)
{
    const int flowCount = 100;

    for (int i = 0; i < flowCount; ++i) {
        Flow *flow = new Flow(QString("flow-%1").arg(i));
        manager->addFlow(flow);
    }

    EXPECT_EQ(manager->getAllFlows().size(), flowCount);
    EXPECT_EQ(manager->getFlowIds().size(), flowCount);

    EXPECT_TRUE(manager->hasFlow("flow-0"));
    EXPECT_TRUE(manager->hasFlow("flow-50"));
    EXPECT_TRUE(manager->hasFlow("flow-99"));
    EXPECT_FALSE(manager->hasFlow("flow-100"));
}

TEST_F(FlowManagerTest, FlowIdConsistency)
{
    Flow *flow = new Flow("consistency-test");
    manager->addFlow(flow);

    EXPECT_EQ(flow->flowId(), "consistency-test");
    EXPECT_EQ(manager->getFlow("consistency-test")->flowId(), "consistency-test");
    EXPECT_TRUE(manager->getFlowIds().contains("consistency-test"));
}

TEST_F(FlowManagerTest, ManagerLifetime)
{
    Flow *flow = nullptr;

    {
        FlowManager tempManager;
        flow = new Flow("temp-flow");
        tempManager.addFlow(flow);

        EXPECT_EQ(tempManager.getAllFlows().size(), 1);
        EXPECT_EQ(flow->parent(), &tempManager);
    }
}

TEST_F(FlowManagerTest, JsonSerializationWithEmptyManager)
{
    QJsonObject json = manager->toJson();

    EXPECT_TRUE(json.contains("flows"));
    EXPECT_TRUE(json.contains("flowCount"));
    EXPECT_EQ(json["flowCount"].toInt(), 0);

    QJsonArray flows = json["flows"].toArray();
    EXPECT_EQ(flows.size(), 0);
}

TEST_F(FlowManagerTest, ClearAfterSerialization)
{
    Flow *flow1 = new Flow("clear-test-1");
    Flow *flow2 = new Flow("clear-test-2");
    manager->addFlow(flow1);
    manager->addFlow(flow2);

    QJsonObject json = manager->toJson();
    EXPECT_EQ(json["flowCount"].toInt(), 2);

    manager->clear();
    EXPECT_EQ(manager->getAllFlows().size(), 0);

    QJsonObject emptyJson = manager->toJson();
    EXPECT_EQ(emptyJson["flowCount"].toInt(), 0);
}

TEST_F(FlowManagerTest, AddFlowAfterDeserialization)
{
    QJsonObject emptyJson;
    emptyJson["flows"] = QJsonArray();
    emptyJson["flowCount"] = 0;

    bool result = manager->fromJson(emptyJson);
    EXPECT_TRUE(result);

    Flow *newFlow = new Flow("post-deserialize-flow");
    manager->addFlow(newFlow);

    EXPECT_EQ(manager->getAllFlows().size(), 1);
    EXPECT_TRUE(manager->hasFlow("post-deserialize-flow"));
}

#include "FlowManagerTest.moc"
