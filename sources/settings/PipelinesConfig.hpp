// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace QodeAssist::Settings {

class PipelinesNotifier : public QObject
{
    Q_OBJECT
public:
    static PipelinesNotifier *instance()
    {
        static PipelinesNotifier notifier;
        return &notifier;
    }

    void notifyChanged() { emit pipelinesChanged(); }

signals:
    void pipelinesChanged();

private:
    PipelinesNotifier() = default;
};

struct PipelineRosters
{
    QStringList codeCompletion;
    QStringList chatAssistant;
    QString chatCompression;
    QString quickRefactor;

    [[nodiscard]] static PipelineRosters defaults();
};

enum class PipelinesLoadStatus { Ok, FileMissing, ParseError, SchemaError };

struct PipelinesLoadResult
{
    PipelineRosters rosters;
    PipelinesLoadStatus status = PipelinesLoadStatus::Ok;
    QString message;
};

class PipelinesConfig
{
public:
    [[nodiscard]] static QString filePath();

    [[nodiscard]] static PipelinesLoadResult load();

    [[nodiscard]] static PipelinesLoadResult loadCached();

    [[nodiscard]] static bool save(const PipelineRosters &rosters, QString *errorOut = nullptr);

    static void setFilePathForTests(const QString &path);
};

} // namespace QodeAssist::Settings
