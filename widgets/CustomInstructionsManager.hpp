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

#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace QodeAssist {

struct CustomInstruction
{
    QString id;
    QString name;
    QString body;
};

class CustomInstructionsManager : public QObject
{
    Q_OBJECT

public:
    static CustomInstructionsManager &instance();

    bool loadInstructions();
    bool saveInstruction(const CustomInstruction &instruction);
    bool deleteInstruction(const QString &id);
    
    QVector<CustomInstruction> instructions() const { return m_instructions; }
    CustomInstruction getInstructionById(const QString &id) const;
    
signals:
    void instructionsChanged();

private:
    explicit CustomInstructionsManager(QObject *parent = nullptr);
    ~CustomInstructionsManager() override = default;
    
    QString getInstructionsDirectory() const;
    bool ensureDirectoryExists() const;
    
    QVector<CustomInstruction> m_instructions;
};

} // namespace QodeAssist

