/* 
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include "CustomInstructionsManager.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <coreplugin/icore.h>

#include <logger/Logger.hpp>

namespace QodeAssist {

CustomInstructionsManager::CustomInstructionsManager(QObject *parent)
    : QObject(parent)
{}

CustomInstructionsManager &CustomInstructionsManager::instance()
{
    static CustomInstructionsManager instance;
    return instance;
}

QString CustomInstructionsManager::getInstructionsDirectory() const
{
    QString path = QString("%1/qodeassist/quick_refactor/instructions")
                       .arg(Core::ICore::userResourcePath().toFSPathString());
    return path;
}

bool CustomInstructionsManager::ensureDirectoryExists() const
{
    QDir dir(getInstructionsDirectory());
    if (!dir.exists()) {
        return dir.mkpath(".");
    }
    return true;
}

bool CustomInstructionsManager::loadInstructions()
{
    m_instructions.clear();

    if (!ensureDirectoryExists()) {
        LOG_MESSAGE("Failed to create instructions directory");
        return false;
    }

    QDir dir(getInstructionsDirectory());
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo &fileInfo : files) {
        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            LOG_MESSAGE(QString("Failed to open instruction file: %1").arg(fileInfo.fileName()));
            continue;
        }

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError) {
            LOG_MESSAGE(
                QString("Failed to parse instruction file %1: %2")
                    .arg(fileInfo.fileName(), error.errorString()));
            continue;
        }

        QJsonObject obj = doc.object();
        CustomInstruction instruction;
        instruction.id = obj["id"].toString();
        instruction.name = obj["name"].toString();
        instruction.body = obj["body"].toString();
        instruction.isDefault = obj["default"].toBool(false);

        if (instruction.id.isEmpty() || instruction.name.isEmpty()) {
            LOG_MESSAGE(QString("Invalid instruction in file: %1").arg(fileInfo.fileName()));
            continue;
        }

        m_instructions.append(instruction);
    }

    LOG_MESSAGE(QString("Loaded %1 custom instructions").arg(m_instructions.size()));
    return true;
}

bool CustomInstructionsManager::saveInstruction(const CustomInstruction &instruction)
{
    if (!ensureDirectoryExists()) {
        LOG_MESSAGE("Failed to create instructions directory");
        return false;
    }

    CustomInstruction newInstruction = instruction;
    QString oldFileName;
    
    if (newInstruction.id.isEmpty()) {
        newInstruction.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    } else {
        // Check if instruction with this ID already exists and get old file name
        for (int i = 0; i < m_instructions.size(); ++i) {
            if (m_instructions[i].id == newInstruction.id) {
                // Build old filename to delete it if name changed
                QString oldName = m_instructions[i].name;
                oldName.replace(' ', '_');
                oldFileName = QString("%1/%2_%3.json")
                    .arg(getInstructionsDirectory(), oldName, newInstruction.id);
                break;
            }
        }
    }

    // If this instruction is marked as default, remove default flag from all others
    if (newInstruction.isDefault) {
        for (int i = 0; i < m_instructions.size(); ++i) {
            if (m_instructions[i].id != newInstruction.id && m_instructions[i].isDefault) {
                m_instructions[i].isDefault = false;
                
                // Update the file for this instruction
                QString sanitizedName = m_instructions[i].name;
                sanitizedName.replace(' ', '_');
                QString otherFileName = QString("%1/%2_%3.json")
                    .arg(getInstructionsDirectory(), sanitizedName, m_instructions[i].id);
                
                QJsonObject otherObj;
                otherObj["id"] = m_instructions[i].id;
                otherObj["name"] = m_instructions[i].name;
                otherObj["body"] = m_instructions[i].body;
                otherObj["default"] = false;
                otherObj["version"] = "0.1";
                
                QFile otherFile(otherFileName);
                if (otherFile.open(QIODevice::WriteOnly)) {
                    otherFile.write(QJsonDocument(otherObj).toJson(QJsonDocument::Indented));
                }
            }
        }
    }

    int existingIndex = -1;
    for (int i = 0; i < m_instructions.size(); ++i) {
        if (m_instructions[i].id == newInstruction.id) {
            existingIndex = i;
            break;
        }
    }

    QJsonObject obj;
    obj["id"] = newInstruction.id;
    obj["name"] = newInstruction.name;
    obj["body"] = newInstruction.body;
    obj["default"] = newInstruction.isDefault;
    obj["version"] = "0.1";

    QJsonDocument doc(obj);
    
    QString sanitizedName = newInstruction.name;
    sanitizedName.replace(' ', '_');
    QString fileName = QString("%1/%2_%3.json")
        .arg(getInstructionsDirectory(), sanitizedName, newInstruction.id);
    
    if (!oldFileName.isEmpty() && oldFileName != fileName) {
        QFile::remove(oldFileName);
    }
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_MESSAGE(QString("Failed to save instruction to file: %1").arg(fileName));
        return false;
    }

    if (file.write(doc.toJson(QJsonDocument::Indented)) == -1) {
        LOG_MESSAGE(QString("Failed to write instruction data: %1").arg(file.errorString()));
        return false;
    }

    if (existingIndex >= 0) {
        m_instructions[existingIndex] = newInstruction;
    } else {
        m_instructions.append(newInstruction);
    }

    emit instructionsChanged();
    LOG_MESSAGE(QString("Saved custom instruction: %1").arg(newInstruction.name));
    return true;
}

bool CustomInstructionsManager::deleteInstruction(const QString &id)
{
    int index = -1;
    for (int i = 0; i < m_instructions.size(); ++i) {
        if (m_instructions[i].id == id) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        LOG_MESSAGE(QString("Instruction not found: %1").arg(id));
        return false;
    }

    QString sanitizedName = m_instructions[index].name;
    sanitizedName.replace(' ', '_');
    QString fileName = QString("%1/%2_%3.json")
        .arg(getInstructionsDirectory(), sanitizedName, id);
    
    QFile file(fileName);
    if (!file.remove()) {
        LOG_MESSAGE(QString("Failed to delete instruction file: %1").arg(fileName));
        return false;
    }

    m_instructions.removeAt(index);
    emit instructionsChanged();
    LOG_MESSAGE(QString("Deleted custom instruction with id: %1").arg(id));
    return true;
}

CustomInstruction CustomInstructionsManager::getInstructionById(const QString &id) const
{
    for (const CustomInstruction &instruction : m_instructions) {
        if (instruction.id == id) {
            return instruction;
        }
    }
    return CustomInstruction();
}

} // namespace QodeAssist

