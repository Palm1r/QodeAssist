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
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "BaseTask.hpp"
#include "Flow.hpp"
#include "FlowManager.hpp"
#include <logger/Logger.hpp>

namespace QodeAssist::Flows {

inline Flow *createTestFileFlow(FlowManager *flowManager)
{
    LOG_MESSAGE("Creating TestFileFlow");

    Flow *flow = new Flow(flowManager);
    flow->setFlowId("TestFileFlow");

    auto taskRegistry = flowManager->taskRegistry();

    BaseTask *task1 = taskRegistry->createTask("Task1", flow);
    if (task1) {
        task1->setTaskId("fileProcessor");
        task1->setParameter(
            "filePath",
            "/Users/palm1r/Projects/QodeAssist/CMakeLists.txt"); // Можно любой существующий файл
        flow->addTask(task1);
    }

    BaseTask *task2 = taskRegistry->createTask("Task2", flow);
    if (task2) {
        task2->setTaskId("analyzer");
        flow->addTask(task2);
    }

    // Соединяем Task1 -> Task2
    if (task1 && task2) {
        flow->addConnection(
            task1, task1->getOutputPort("file_path"), task2, task2->getInputPort("file_path"));
        flow->addConnection(
            task1, task1->getOutputPort("file_size"), task2, task2->getInputPort("file_size"));
        flow->addConnection(
            task1,
            task1->getOutputPort("last_modified"),
            task2,
            task2->getInputPort("last_modified"));
        flow->addConnection(
            task1, task1->getOutputPort("completed"), task2, task2->getInputPort("completed"));
    }

    LOG_MESSAGE("SimpleTextFlow created successfully with 1 task");
    return flow;
}

} // namespace QodeAssist::Flows
