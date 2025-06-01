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
#include <memory>
#include <QObject>
#include <QString>

#include "BaseTask.hpp"
#include "TaskRegistry.hpp"

using namespace QodeAssist;

class TestTask1 : public BaseTask
{
    Q_OBJECT

public:
    explicit TestTask1(QObject *parent = nullptr)
        : BaseTask(parent)
    {
        setTaskId("test-task-1");
    }

    TaskState execute() override { return TaskState::Success; }
};

class TestTask2 : public BaseTask
{
    Q_OBJECT

public:
    explicit TestTask2(QObject *parent = nullptr)
        : BaseTask(parent)
    {
        setTaskId("test-task-2");
    }

    TaskState execute() override { return TaskState::Failed; }
};

class TaskWithConstructorParams : public BaseTask
{
    Q_OBJECT

public:
    explicit TaskWithConstructorParams(
        const QString &customId = "param-task", QObject *parent = nullptr)
        : BaseTask(parent)
    {
        setTaskId(customId);
    }

    TaskState execute() override { return TaskState::Success; }
};

class TaskRegistryTest : public testing::Test
{
protected:
    void SetUp() override { registry = std::make_unique<TaskRegistry>(); }

    void TearDown() override { registry.reset(); }

    std::unique_ptr<TaskRegistry> registry;
};

TEST_F(TaskRegistryTest, EmptyRegistryReturnsNull)
{
    BaseTask *task = registry->createTask("NonExistentTask");
    EXPECT_EQ(task, nullptr);
}

TEST_F(TaskRegistryTest, RegisterSingleTask)
{
    registry->registerTask<TestTask1>("TestTask1");

    BaseTask *task = registry->createTask("TestTask1");
    EXPECT_NE(task, nullptr);

    TestTask1 *testTask = dynamic_cast<TestTask1 *>(task);
    EXPECT_NE(testTask, nullptr);
    EXPECT_EQ(testTask->taskId(), "test-task-1");

    delete task;
}

TEST_F(TaskRegistryTest, RegisterMultipleTasks)
{
    registry->registerTask<TestTask1>("TestTask1");
    registry->registerTask<TestTask2>("TestTask2");

    BaseTask *task1 = registry->createTask("TestTask1");
    BaseTask *task2 = registry->createTask("TestTask2");

    EXPECT_NE(task1, nullptr);
    EXPECT_NE(task2, nullptr);

    TestTask1 *testTask1 = dynamic_cast<TestTask1 *>(task1);
    TestTask2 *testTask2 = dynamic_cast<TestTask2 *>(task2);

    EXPECT_NE(testTask1, nullptr);
    EXPECT_NE(testTask2, nullptr);

    EXPECT_EQ(testTask1->taskId(), "test-task-1");
    EXPECT_EQ(testTask2->taskId(), "test-task-2");

    delete task1;
    delete task2;
}

TEST_F(TaskRegistryTest, CreateNonExistentTask)
{
    registry->registerTask<TestTask1>("TestTask1");

    BaseTask *existingTask = registry->createTask("TestTask1");
    BaseTask *nonExistentTask = registry->createTask("NonExistentTask");

    EXPECT_NE(existingTask, nullptr);
    EXPECT_EQ(nonExistentTask, nullptr);

    delete existingTask;
}

TEST_F(TaskRegistryTest, OverwriteTaskRegistration)
{
    registry->registerTask<TestTask1>("CommonName");

    BaseTask *task1 = registry->createTask("CommonName");
    EXPECT_NE(task1, nullptr);

    TestTask1 *testTask1 = dynamic_cast<TestTask1 *>(task1);
    EXPECT_NE(testTask1, nullptr);
    delete task1;

    registry->registerTask<TestTask2>("CommonName");

    BaseTask *task2 = registry->createTask("CommonName");
    EXPECT_NE(task2, nullptr);

    TestTask2 *testTask2 = dynamic_cast<TestTask2 *>(task2);
    EXPECT_NE(testTask2, nullptr);

    TestTask1 *shouldBeNull = dynamic_cast<TestTask1 *>(task2);
    EXPECT_EQ(shouldBeNull, nullptr);

    delete task2;
}

TEST_F(TaskRegistryTest, CreateMultipleInstancesOfSameTask)
{
    registry->registerTask<TestTask1>("TestTask1");

    BaseTask *task1 = registry->createTask("TestTask1");
    BaseTask *task2 = registry->createTask("TestTask1");
    BaseTask *task3 = registry->createTask("TestTask1");

    EXPECT_NE(task1, nullptr);
    EXPECT_NE(task2, nullptr);
    EXPECT_NE(task3, nullptr);

    EXPECT_NE(task1, task2);
    EXPECT_NE(task2, task3);
    EXPECT_NE(task1, task3);

    EXPECT_NE(dynamic_cast<TestTask1 *>(task1), nullptr);
    EXPECT_NE(dynamic_cast<TestTask1 *>(task2), nullptr);
    EXPECT_NE(dynamic_cast<TestTask1 *>(task3), nullptr);

    delete task1;
    delete task2;
    delete task3;
}

TEST_F(TaskRegistryTest, EmptyTaskName)
{
    registry->registerTask<TestTask1>("");

    BaseTask *task = registry->createTask("");
    EXPECT_NE(task, nullptr);

    delete task;
}

TEST_F(TaskRegistryTest, TaskNameWithSpecialCharacters)
{
    QString specialName = "Task-With_Special.Characters@123";
    registry->registerTask<TestTask1>(specialName);

    BaseTask *task = registry->createTask(specialName);
    EXPECT_NE(task, nullptr);

    delete task;
}

TEST_F(TaskRegistryTest, CreateWithDefaults)
{
    TaskRegistry defaultRegistry = TaskRegistry::createWithDefaults();

    BaseTask *task1 = defaultRegistry.createTask("Task1");
    BaseTask *task2 = defaultRegistry.createTask("Task2");
    BaseTask *simpleLLMTask = defaultRegistry.createTask("SimpleLLMTask");

    EXPECT_NE(task1, nullptr);
    EXPECT_NE(task2, nullptr);
    EXPECT_NE(simpleLLMTask, nullptr);

    delete task1;
    delete task2;
    delete simpleLLMTask;
}

TEST_F(TaskRegistryTest, CaseSensitiveTaskNames)
{
    registry->registerTask<TestTask1>("TestTask");
    registry->registerTask<TestTask2>("testtask");
    registry->registerTask<TestTask1>("TESTTASK");

    BaseTask *task1 = registry->createTask("TestTask");
    BaseTask *task2 = registry->createTask("testtask");
    BaseTask *task3 = registry->createTask("TESTTASK");
    BaseTask *task4 = registry->createTask("TeStTaSk");

    EXPECT_NE(task1, nullptr);
    EXPECT_NE(task2, nullptr);
    EXPECT_NE(task3, nullptr);
    EXPECT_EQ(task4, nullptr);

    delete task1;
    delete task2;
    delete task3;
}

TEST_F(TaskRegistryTest, TaskRegistrationOrder)
{
    registry->registerTask<TestTask2>("Second");
    registry->registerTask<TestTask1>("First");

    BaseTask *firstTask = registry->createTask("First");
    BaseTask *secondTask = registry->createTask("Second");

    EXPECT_NE(firstTask, nullptr);
    EXPECT_NE(secondTask, nullptr);

    TestTask1 *test1 = dynamic_cast<TestTask1 *>(firstTask);
    TestTask2 *test2 = dynamic_cast<TestTask2 *>(secondTask);

    EXPECT_NE(test1, nullptr);
    EXPECT_NE(test2, nullptr);

    delete firstTask;
    delete secondTask;
}

TEST_F(TaskRegistryTest, LargeNumberOfTasks)
{
    const int taskCount = 100;

    for (int i = 0; i < taskCount; ++i) {
        QString taskName = QString("TestTask_%1").arg(i);
        registry->registerTask<TestTask1>(taskName);
    }

    QList<BaseTask *> tasks;
    for (int i = 0; i < taskCount; ++i) {
        QString taskName = QString("TestTask_%1").arg(i);
        BaseTask *task = registry->createTask(taskName);
        EXPECT_NE(task, nullptr);
        tasks.append(task);
    }

    EXPECT_EQ(tasks.size(), taskCount);
    for (BaseTask *task : tasks) {
        EXPECT_NE(task, nullptr);
        EXPECT_NE(dynamic_cast<TestTask1 *>(task), nullptr);
    }

    qDeleteAll(tasks);
}

TEST_F(TaskRegistryTest, TaskCreationWithCustomConstructor)
{
    registry->registerTask<TaskWithConstructorParams>("CustomTask");

    BaseTask *task = registry->createTask("CustomTask");
    EXPECT_NE(task, nullptr);

    EXPECT_EQ(task->taskId(), "param-task");

    delete task;
}

TEST_F(TaskRegistryTest, RegistryLifetime)
{
    BaseTask *task = nullptr;

    {
        TaskRegistry temporaryRegistry;
        temporaryRegistry.registerTask<TestTask1>("TempTask");
        task = temporaryRegistry.createTask("TempTask");
        EXPECT_NE(task, nullptr);
    }

    EXPECT_NE(task, nullptr);
    EXPECT_EQ(task->taskId(), "test-task-1");

    delete task;
}

TEST_F(TaskRegistryTest, CopyAndMoveSemantics)
{
    registry->registerTask<TestTask1>("OriginalTask");

    TaskRegistry copiedRegistry = *registry;
    BaseTask *task = copiedRegistry.createTask("OriginalTask");
    EXPECT_NE(task, nullptr);

    delete task;
}

#include "TaskRegistryTest.moc"
