// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
    bool isDefault = false;
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

