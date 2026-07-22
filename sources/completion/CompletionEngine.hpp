// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QObject>
#include <QString>

namespace QodeAssist {

struct CompletionContext
{
    QString filePath;
    int line = 0;
    int column = 0;
};

class CompletionEngine : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual void request(quint64 requestId, const CompletionContext &context) = 0;
    virtual void cancel(quint64 requestId) = 0;

signals:
    void completionReady(quint64 requestId, const QString &text);
    void completionFailed(quint64 requestId, const QString &error);
};

} // namespace QodeAssist
