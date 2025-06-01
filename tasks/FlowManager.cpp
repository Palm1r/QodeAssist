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

#include "FlowManager.hpp"
#include <logger/Logger.hpp>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

namespace QodeAssist {

FlowManager::FlowManager(QObject *parent)
    : QObject(parent)
{
    LOG_MESSAGE("FlowManager created");
}

FlowManager::~FlowManager()
{
    clear();
}

void FlowManager::addFlow(Flow *flow)
{
    if (!flow) {
        LOG_MESSAGE("FlowManager::addFlow - null flow provided");
        return;
    }

    const QString flowId = flow->flowId();
    if (flowId.isEmpty()) {
        LOG_MESSAGE("FlowManager::addFlow - flow has empty ID");
        return;
    }

    if (m_flows.contains(flowId)) {
        LOG_MESSAGE(QString("FlowManager::addFlow - flow with ID '%1' already exists, replacing")
                        .arg(flowId));
        delete m_flows[flowId];
    }

    m_flows[flowId] = flow;
    flow->setParent(this);

    LOG_MESSAGE(QString("FlowManager::addFlow - added flow '%1'").arg(flowId));
}

void FlowManager::removeFlow(const QString &flowId)
{
    if (!m_flows.contains(flowId)) {
        LOG_MESSAGE(QString("FlowManager::removeFlow - flow '%1' not found").arg(flowId));
        return;
    }

    Flow *flow = m_flows.take(flowId);
    delete flow;

    LOG_MESSAGE(QString("FlowManager::removeFlow - removed flow '%1'").arg(flowId));
}

Flow *FlowManager::getFlow(const QString &flowId) const
{
    return m_flows.value(flowId, nullptr);
}

QList<Flow *> FlowManager::getAllFlows() const
{
    return m_flows.values();
}

QStringList FlowManager::getFlowIds() const
{
    return m_flows.keys();
}

bool FlowManager::hasFlow(const QString &flowId) const
{
    return m_flows.contains(flowId);
}

void FlowManager::clear()
{
    LOG_MESSAGE(QString("FlowManager::clear - removing %1 flows").arg(m_flows.size()));

    qDeleteAll(m_flows);
    m_flows.clear();
}

QJsonObject FlowManager::toJson() const
{
    QJsonObject managerObj;

    QJsonArray flowsArray;
    for (auto it = m_flows.constBegin(); it != m_flows.constEnd(); ++it) {
        Flow *flow = it.value();
        flowsArray.append(flow->toJson());
    }

    managerObj["flows"] = flowsArray;
    managerObj["flowCount"] = m_flows.size();

    LOG_MESSAGE(QString("FlowManager::toJson - serialized %1 flows").arg(m_flows.size()));

    return managerObj;
}

bool FlowManager::fromJson(const QJsonObject &json)
{
    clear();

    if (!json.contains("flows")) {
        LOG_MESSAGE("FlowManager::fromJson - no 'flows' array found");
        return false;
    }

    QJsonArray flowsArray = json["flows"].toArray();
    int loadedCount = 0;

    for (const QJsonValue &flowValue : flowsArray) {
        QJsonObject flowObj = flowValue.toObject();

        Flow *flow = new Flow("", this);
        if (flow->fromJson(flowObj)) {
            addFlow(flow);
            loadedCount++;
        } else {
            LOG_MESSAGE("FlowManager::fromJson - failed to load flow");
            delete flow;
        }
    }

    LOG_MESSAGE(QString("FlowManager::fromJson - loaded %1 flows").arg(loadedCount));

    return loadedCount > 0 || flowsArray.isEmpty();
}

bool FlowManager::saveToFile(const QString &filePath) const
{
    if (filePath.isEmpty()) {
        LOG_MESSAGE("FlowManager::saveToFile - empty file path");
        return false;
    }

    QJsonDocument doc(toJson());
    QByteArray jsonData = doc.toJson();

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_MESSAGE(QString("FlowManager::saveToFile - failed to open file: %1").arg(filePath));
        return false;
    }

    qint64 bytesWritten = file.write(jsonData);
    if (bytesWritten != jsonData.size()) {
        LOG_MESSAGE(QString("FlowManager::saveToFile - write error for file: %1").arg(filePath));
        return false;
    }

    LOG_MESSAGE(
        QString("FlowManager::saveToFile - saved %1 flows to: %2").arg(m_flows.size()).arg(filePath));

    return true;
}

bool FlowManager::loadFromFile(const QString &filePath)
{
    if (filePath.isEmpty()) {
        LOG_MESSAGE("FlowManager::loadFromFile - empty file path");
        return false;
    }

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        LOG_MESSAGE(QString("FlowManager::loadFromFile - file does not exist: %1").arg(filePath));
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_MESSAGE(QString("FlowManager::loadFromFile - failed to open file: %1").arg(filePath));
        return false;
    }

    QByteArray jsonData = file.readAll();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        LOG_MESSAGE(QString("FlowManager::loadFromFile - JSON parse error: %1")
                        .arg(parseError.errorString()));
        return false;
    }

    bool result = fromJson(doc.object());

    if (result) {
        LOG_MESSAGE(
            QString("FlowManager::loadFromFile - successfully loaded from: %1").arg(filePath));
    } else {
        LOG_MESSAGE(QString("FlowManager::loadFromFile - failed to load from: %1").arg(filePath));
    }

    return result;
}

} // namespace QodeAssist
