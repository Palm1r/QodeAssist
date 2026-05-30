// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <functional>

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include "ProviderInstance.hpp"

class QNetworkAccessManager;
class QNetworkReply;
class QProcessEnvironment;
class QTimer;

namespace Utils { class Process; }

namespace QodeAssist::Providers {

class ProviderLauncher : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ProviderLauncher)
public:
    enum State : quint8 {
        Idle,      // no process running
        Starting,  // QProcess::start invoked, waiting for it to be alive
        Probing,   // process alive, polling ready_url
        Ready,     // ready probe succeeded (or no probe configured)
        Stopping,  // termination requested; waiting for QProcess to exit
        Failed,    // process exited unexpectedly or readiness probe gave up
    };
    Q_ENUM(State)

    explicit ProviderLauncher(QObject *parent = nullptr);
    ~ProviderLauncher() override;


    void start(const QString &instanceName, const LaunchConfig &cfg);
    void stop(const QString &instanceName);
    void restart(const QString &instanceName, const LaunchConfig &cfg);

    [[nodiscard]] State state(const QString &instanceName) const;
    [[nodiscard]] QString lastError(const QString &instanceName) const;
    [[nodiscard]] QByteArray scrollback(const QString &instanceName) const;

signals:
    void stateChanged(const QString &instanceName, State newState);
    void bytesReceived(const QString &instanceName, const QByteArray &chunk);

private:
    struct Slot;
    void teardownSlot(Slot *slot);
    void launchProcess(Slot *slot);
    void launchDetached(Slot *slot);
    void appendScrollback(Slot *slot, const QByteArray &chunk);
    void scheduleReadyProbe(Slot *slot);
    void runReadyProbe(Slot *slot);
    void probeOnceAsync(Slot *slot, int expectedGeneration, const QString &url,
                        std::function<void(bool)> onResult);
    void appendLog(Slot *slot, const QString &line);
    void changeState(Slot *slot, State newState);
    QString expandVars(const QString &input, const Slot *slot) const;
    QStringList expandVars(const QStringList &args, const Slot *slot) const;
    static QString expandOne(const QString &input, const Slot *slot,
                             const QProcessEnvironment &sys);
    static void killByPid(qint64 pid);

    QHash<QString, Slot *> m_slots;
    QNetworkAccessManager *m_nam = nullptr;
};

} // namespace QodeAssist::Providers
