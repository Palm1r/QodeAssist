// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ProviderLauncher.hpp"

#include <QDir>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QUrl>

#include <utils/commandline.h>
#include <utils/environment.h>
#include <utils/filepath.h>
#include <utils/processinterface.h>
#include <utils/qtcprocess.h>

#include "Logger.hpp"

#include <algorithm>
#include <chrono>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <signal.h>
#endif

namespace QodeAssist::Providers {

namespace {

Q_LOGGING_CATEGORY(launcherLog, "qodeassist.providerlauncher")

constexpr std::chrono::milliseconds kProbeInterval{500};
constexpr std::chrono::milliseconds kProbeTransferTimeout{2000};
constexpr std::chrono::milliseconds kAdoptionTransferTimeout{1500};
constexpr std::chrono::milliseconds kStartTimeout{2000};
constexpr int kScrollbackBytesMax = 1 * 1024 * 1024; // 1 MiB cap per slot

} // namespace

struct ProviderLauncher::Slot
{
    QString name;
    LaunchConfig cfg;
    State state = Idle;
    Utils::Process *process = nullptr;
    qint64 detachedPid = 0;
    bool adoptedExternal = false;
    bool started = false;
    QTimer *probeTimer = nullptr;
    QTimer *startTimer = nullptr; // fail-fast timer for QProcess::started
    QElapsedTimer probeStart;
    QPointer<QNetworkReply> probeReply;
    QList<QPointer<QNetworkReply>> oneShotProbes;
    int generation = 0;
    QString lastError;
    QByteArray scrollback;
};

ProviderLauncher::ProviderLauncher(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    connect(m_nam, &QNetworkAccessManager::sslErrors, this,
            [this](QNetworkReply *reply, const QList<QSslError> &errors) {
                QStringList msgs;
                msgs.reserve(errors.size());
                for (const QSslError &e : errors)
                    msgs.append(e.errorString());
                qCWarning(launcherLog).noquote()
                    << "SSL errors on probe to" << reply->url().toString() << ":"
                    << msgs.join(QStringLiteral("; "));
            });
}

ProviderLauncher::~ProviderLauncher()
{
    m_nam->disconnect(this);
    for (Slot *slot : m_slots) {
        if (slot->cfg.detach) {
            if (slot->probeTimer) {
                slot->probeTimer->stop();
                slot->probeTimer->deleteLater();
                slot->probeTimer = nullptr;
            }
            if (slot->probeReply) {
                slot->probeReply->abort();
                slot->probeReply->deleteLater();
                slot->probeReply.clear();
            }
        } else {
            teardownSlot(slot);
        }
        delete slot;
    }
    m_slots.clear();
}

void ProviderLauncher::start(const QString &instanceName, const LaunchConfig &cfg)
{
    if (instanceName.isEmpty() || cfg.isEmpty())
        return;
    Slot *slot = m_slots.value(instanceName, nullptr);
    if (slot) {
        if (slot->state == Starting || slot->state == Probing || slot->state == Ready) {
            slot->cfg = cfg;
            return;
        }
        teardownSlot(slot);
    } else {
        slot = new Slot;
        slot->name = instanceName;
        m_slots.insert(instanceName, slot);
    }
    slot->cfg = cfg;
    slot->scrollback.clear();
    slot->lastError.clear();
    slot->detachedPid = 0;
    slot->adoptedExternal = false;
    slot->started = false;
    ++slot->generation;
    const int gen = slot->generation;

    if (!cfg.readyUrl.isEmpty()) {
        changeState(slot, Starting);
        const QString name = instanceName;
        const QString readyUrl = cfg.readyUrl;
        probeOnceAsync(slot, gen, readyUrl, [this, name, readyUrl](bool ok) {
            Slot *s = m_slots.value(name, nullptr);
            if (!s || s->state != Starting)
                return;
            if (ok) {
                s->adoptedExternal = true;
                s->detachedPid = 0;
                appendLog(s, QStringLiteral(
                    "[adopt] %1 is already up — reusing the running process (no pid).")
                              .arg(readyUrl));
                changeState(s, Ready);
                return;
            }
            if (s->cfg.detach)
                launchDetached(s);
            else
                launchProcess(s);
        });
        return;
    }

    if (cfg.detach)
        launchDetached(slot);
    else
        launchProcess(slot);
}

void ProviderLauncher::stop(const QString &instanceName)
{
    Slot *slot = m_slots.value(instanceName, nullptr);
    if (!slot)
        return;
    if (slot->state == Idle || slot->state == Failed) {
        changeState(slot, Idle);
        return;
    }
    changeState(slot, Stopping);
    if (slot->cfg.detach) {
        const qint64 pid = slot->detachedPid;
        const QString readyUrl = slot->cfg.readyUrl;
        if (slot->probeTimer) {
            slot->probeTimer->stop();
            slot->probeTimer->deleteLater();
            slot->probeTimer = nullptr;
        }
        if (slot->probeReply) {
            slot->probeReply->abort();
            slot->probeReply->deleteLater();
            slot->probeReply.clear();
        }
        slot->detachedPid = 0;
        slot->adoptedExternal = false;

        if (pid <= 0) {
            appendLog(slot, QStringLiteral(
                "[stop] no pid recorded (process was adopted via probe) — "
                "cannot terminate from the plugin; kill manually if needed."));
            changeState(slot, Idle);
            return;
        }

        if (readyUrl.isEmpty()) {
            appendLog(slot, QStringLiteral("[stop] SIGTERM pid=%1").arg(pid));
            killByPid(pid);
            changeState(slot, Idle);
            return;
        }
        const QString name = instanceName;
        ++slot->generation;
        const int gen = slot->generation;
        probeOnceAsync(slot, gen, readyUrl, [this, name, pid](bool stillUp) {
            Slot *s = m_slots.value(name, nullptr);
            if (!s)
                return;
            if (stillUp) {
                appendLog(s, QStringLiteral("[stop] SIGTERM pid=%1").arg(pid));
                killByPid(pid);
            } else {
                appendLog(s, QStringLiteral(
                    "[stop] pid=%1 no longer responsive on ready_url — "
                    "skipping kill to avoid hitting a reused PID.").arg(pid));
            }
            if (s->state == Stopping)
                changeState(s, Idle);
        });
        return;
    }
    teardownSlot(slot);
    changeState(slot, Idle);
}

void ProviderLauncher::restart(const QString &instanceName, const LaunchConfig &cfg)
{
    stop(instanceName);
    start(instanceName, cfg);
}

ProviderLauncher::State ProviderLauncher::state(const QString &instanceName) const
{
    const Slot *slot = m_slots.value(instanceName, nullptr);
    return slot ? slot->state : Idle;
}

QString ProviderLauncher::lastError(const QString &instanceName) const
{
    const Slot *slot = m_slots.value(instanceName, nullptr);
    return slot ? slot->lastError : QString{};
}

QByteArray ProviderLauncher::scrollback(const QString &instanceName) const
{
    const Slot *slot = m_slots.value(instanceName, nullptr);
    return slot ? slot->scrollback : QByteArray{};
}

void ProviderLauncher::launchProcess(Slot *slot)
{
    const LaunchConfig &cfg = slot->cfg;
    const QString command = expandVars(cfg.command, slot);
    const QStringList args = expandVars(cfg.args, slot);
    const QString cwd = cfg.cwd.isEmpty() ? QDir::homePath() : expandVars(cfg.cwd, slot);
    const QString name = slot->name;

    auto *proc = new Utils::Process(this);
    slot->process = proc;

    Utils::Environment env = Utils::Environment::systemEnvironment();
    env.set(QStringLiteral("PROVIDER_NAME"), slot->name);
    for (auto it = cfg.env.constBegin(); it != cfg.env.constEnd(); ++it)
        env.set(it.key(), it.value());
    proc->setEnvironment(env);
    proc->setWorkingDirectory(Utils::FilePath::fromString(cwd));
    proc->setCommand(Utils::CommandLine{Utils::FilePath::fromString(command), args});

    proc->setPtyData(Utils::Pty::Data{});

    connect(proc, &Utils::Process::readyReadStandardOutput, this, [this, name] {
        Slot *s = m_slots.value(name, nullptr);
        if (!s || !s->process) return;
        const QByteArray chunk = s->process->readAllRawStandardOutput();
        if (!chunk.isEmpty()) {
            appendScrollback(s, chunk);
            emit bytesReceived(s->name, chunk);
        }
    });
    connect(proc, &Utils::Process::readyReadStandardError, this, [this, name] {
        Slot *s = m_slots.value(name, nullptr);
        if (!s || !s->process) return;
        const QByteArray chunk = s->process->readAllRawStandardError();
        if (!chunk.isEmpty()) {
            appendScrollback(s, chunk);
            emit bytesReceived(s->name, chunk);
        }
    });
    connect(proc, &Utils::Process::started, this, [this, name] {
        Slot *s = m_slots.value(name, nullptr);
        if (!s)
            return;
        s->started = true;
        if (s->startTimer) {
            s->startTimer->stop();
            s->startTimer->deleteLater();
            s->startTimer = nullptr;
        }
        if (s->state != Starting)
            return;
        if (s->cfg.readyUrl.isEmpty()) {
            changeState(s, Ready);
            return;
        }
        s->probeStart.start();
        changeState(s, Probing);
        scheduleReadyProbe(s);
    });

    connect(proc, &Utils::Process::done, this, [this, name] {
        Slot *s = m_slots.value(name, nullptr);
        if (!s || !s->process) return;
        const QByteArray tailOut = s->process->readAllRawStandardOutput();
        const QByteArray tailErr = s->process->readAllRawStandardError();
        if (!tailOut.isEmpty()) { appendScrollback(s, tailOut); emit bytesReceived(s->name, tailOut); }
        if (!tailErr.isEmpty()) { appendScrollback(s, tailErr); emit bytesReceived(s->name, tailErr); }

        const int code = s->process->exitCode();
        const QProcess::ExitStatus status = s->process->exitStatus();
        appendLog(s, QStringLiteral("[exit] code=%1 status=%2")
                            .arg(code)
                            .arg(status == QProcess::NormalExit ? "normal" : "crashed"));
        const State prev = s->state;
        teardownSlot(s);
        if (prev != Stopping && code != 0) {
            s->lastError = QStringLiteral("Process exited (code %1)").arg(code);
            changeState(s, Failed);
        } else {
            changeState(s, Idle);
        }
    });

    appendLog(slot, QStringLiteral("[spawn] %1 %2")
                        .arg(command, args.join(QLatin1Char(' '))));
    changeState(slot, Starting);
    proc->start();

    if (slot->startTimer) {
        slot->startTimer->stop();
        slot->startTimer->deleteLater();
    }
    slot->startTimer = new QTimer(this);
    slot->startTimer->setSingleShot(true);
    const QString slotName = slot->name;
    connect(slot->startTimer, &QTimer::timeout, this, [this, slotName] {
        Slot *s = m_slots.value(slotName, nullptr);
        if (!s || s->started || s->state != Starting)
            return;
        s->lastError = s->process && !s->process->errorString().isEmpty()
                           ? s->process->errorString()
                           : QStringLiteral("Process failed to start");
        appendLog(s, QStringLiteral("[error] %1").arg(s->lastError));
        teardownSlot(s);
        changeState(s, Failed);
    });
    slot->startTimer->start(kStartTimeout);
}

void ProviderLauncher::launchDetached(Slot *slot)
{
    const LaunchConfig &cfg = slot->cfg;
    const QString command = expandVars(cfg.command, slot);
    const QStringList args = expandVars(cfg.args, slot);
    const QString cwd = cfg.cwd.isEmpty() ? QDir::homePath() : expandVars(cfg.cwd, slot);

    appendLog(slot, QStringLiteral("[spawn-detached] %1 %2")
                        .arg(command, args.join(QLatin1Char(' '))));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PROVIDER_NAME"), slot->name);
    for (auto it = cfg.env.constBegin(); it != cfg.env.constEnd(); ++it)
        env.insert(it.key(), it.value());

    QProcess tmp;
    tmp.setProgram(command);
    tmp.setArguments(args);
    tmp.setWorkingDirectory(cwd);
    tmp.setProcessEnvironment(env);
    tmp.setStandardOutputFile(QProcess::nullDevice());
    tmp.setStandardErrorFile(QProcess::nullDevice());
    qint64 pid = 0;
    const bool ok = tmp.startDetached(&pid);
    if (!ok || pid <= 0) {
        slot->lastError = tmp.errorString().isEmpty()
                              ? QStringLiteral("Detached spawn failed")
                              : tmp.errorString();
        appendLog(slot, QStringLiteral("[error] %1").arg(slot->lastError));
        changeState(slot, Failed);
        return;
    }
    slot->detachedPid = pid;
    appendLog(slot, QStringLiteral("[detached] pid=%1 (stdout/stderr discarded)").arg(pid));

    if (cfg.readyUrl.isEmpty()) {
        changeState(slot, Ready);
        return;
    }
    slot->probeStart.start();
    changeState(slot, Probing);
    scheduleReadyProbe(slot);
}

void ProviderLauncher::probeOnceAsync(
    Slot *slot, int expectedGeneration, const QString &url,
    std::function<void(bool)> onResult)
{
    QNetworkRequest req(QUrl{url});
    req.setTransferTimeout(kAdoptionTransferTimeout);
    QNetworkReply *reply = m_nam->get(req);
    if (slot)
        slot->oneShotProbes.append(QPointer<QNetworkReply>(reply));
    const QString name = slot ? slot->name : QString{};
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, name, expectedGeneration, cb = std::move(onResult)] {
                reply->deleteLater();
                Slot *s = m_slots.value(name, nullptr);
                if (s) {
                    s->oneShotProbes.removeAll(QPointer<QNetworkReply>(reply));
                    if (s->generation != expectedGeneration)
                        return;
                }
                const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const bool ok = reply->error() == QNetworkReply::NoError && http >= 200 && http < 300;
                cb(ok);
            });
}

void ProviderLauncher::killByPid(qint64 pid)
{
    if (pid <= 0)
        return;
#ifdef Q_OS_WIN
    HANDLE h = ::OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (h) {
        ::TerminateProcess(h, 1);
        ::CloseHandle(h);
    }
#else
    ::kill(static_cast<pid_t>(pid), SIGTERM);
#endif
}

void ProviderLauncher::scheduleReadyProbe(Slot *slot)
{
    if (!slot->probeTimer) {
        const QString name = slot->name;
        slot->probeTimer = new QTimer(this);
        slot->probeTimer->setSingleShot(true);
        connect(slot->probeTimer, &QTimer::timeout, this, [this, name] {
            if (Slot *s = m_slots.value(name, nullptr))
                runReadyProbe(s);
        });
    }
    slot->probeTimer->start(kProbeInterval);
}

void ProviderLauncher::runReadyProbe(Slot *slot)
{
    if (!slot || slot->state != Probing)
        return;
    const auto elapsed = std::chrono::milliseconds{slot->probeStart.elapsed()};
    if (elapsed > slot->cfg.readyTimeout) {
        slot->lastError = QStringLiteral("Ready probe timed out after %1 s")
                              .arg(slot->cfg.readyTimeout.count());
        appendLog(slot, QStringLiteral("[probe] timeout — %1").arg(slot->lastError));
        changeState(slot, Failed);
        teardownSlot(slot);
        return;
    }
    QNetworkRequest req(QUrl{slot->cfg.readyUrl});
    req.setTransferTimeout(kProbeTransferTimeout);
    slot->probeReply = m_nam->get(req);
    const QString name = slot->name;
    connect(slot->probeReply, &QNetworkReply::finished, this, [this, name] {
        Slot *s = m_slots.value(name, nullptr);
        if (!s || !s->probeReply)
            return;
        QNetworkReply *reply = s->probeReply;
        s->probeReply.clear();
        reply->deleteLater();
        if (s->state != Probing)
            return;
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && http >= 200 && http < 300) {
            appendLog(s, QStringLiteral("[probe] %1 → %2 OK").arg(s->cfg.readyUrl).arg(http));
            changeState(s, Ready);
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            appendLog(s, QStringLiteral("[probe] %1 → %2")
                              .arg(s->cfg.readyUrl, reply->errorString()));
        }
        scheduleReadyProbe(s);
    });
}

void ProviderLauncher::teardownSlot(Slot *slot)
{
    if (!slot)
        return;

    ++slot->generation;
    if (slot->probeTimer) {
        slot->probeTimer->stop();
        slot->probeTimer->deleteLater();
        slot->probeTimer = nullptr;
    }
    if (slot->startTimer) {
        slot->startTimer->stop();
        slot->startTimer->deleteLater();
        slot->startTimer = nullptr;
    }
    if (slot->probeReply) {
        slot->probeReply->abort();
        slot->probeReply->deleteLater();
        slot->probeReply.clear();
    }
    for (const QPointer<QNetworkReply> &probe : slot->oneShotProbes) {
        if (probe) {
            probe->abort();
            probe->deleteLater();
        }
    }
    slot->oneShotProbes.clear();
    if (slot->process) {
        Utils::Process *p = slot->process;
        slot->process = nullptr;
        p->disconnect(this);
        if (p->state() == QProcess::NotRunning) {
            p->deleteLater();
        } else {
            QObject::connect(p, &Utils::Process::done, p, &QObject::deleteLater);
            QTimer::singleShot(std::chrono::seconds{15}, p, &QObject::deleteLater);
            p->stop();
        }
    }
}

void ProviderLauncher::appendLog(Slot *slot, const QString &line)
{
    if (line.isEmpty())
        return;
    const QByteArray bytes = (line + QStringLiteral("\r\n")).toUtf8();
    appendScrollback(slot, bytes);
    emit bytesReceived(slot->name, bytes);
}

void ProviderLauncher::appendScrollback(Slot *slot, const QByteArray &chunk)
{
    if (chunk.isEmpty())
        return;
    slot->scrollback.append(chunk);
    if (slot->scrollback.size() > kScrollbackBytesMax) {
        const int over = slot->scrollback.size() - kScrollbackBytesMax;
        slot->scrollback.remove(0, over);
    }
}

void ProviderLauncher::changeState(Slot *slot, State newState)
{
    if (slot->state == newState)
        return;
    slot->state = newState;
    const QString name = slot->name;
    qCDebug(launcherLog).noquote() << name << "→ state" << newState;
    emit stateChanged(name, newState);
}

QString ProviderLauncher::expandOne(
    const QString &input, const Slot *slot, const QProcessEnvironment &sys)
{
    if (!input.contains(QLatin1String("${")))
        return input;
    QString out = input;
    int searchFrom = 0;
    while (searchFrom < out.size()) {
        const int open = out.indexOf(QLatin1String("${"), searchFrom);
        if (open < 0)
            break;
        const int close = out.indexOf(QLatin1Char('}'), open + 2);
        if (close < 0)
            break;
        const QString key = out.mid(open + 2, close - open - 2);
        QString value;
        if (slot && slot->cfg.env.contains(key))
            value = slot->cfg.env.value(key);
        else if (key == QLatin1String("PROVIDER_NAME") && slot)
            value = slot->name;
        else if (sys.contains(key))
            value = sys.value(key);
        out.replace(open, close - open + 1, value);
        searchFrom = open + value.size();
    }
    return out;
}

QString ProviderLauncher::expandVars(const QString &input, const Slot *slot) const
{
    if (!input.contains(QLatin1String("${")))
        return input;
    return expandOne(input, slot, QProcessEnvironment::systemEnvironment());
}

QStringList ProviderLauncher::expandVars(const QStringList &args, const Slot *slot) const
{
    if (args.isEmpty())
        return {};

    const QProcessEnvironment sys = QProcessEnvironment::systemEnvironment();
    QStringList out;
    out.reserve(args.size());
    for (const QString &a : args)
        out.append(expandOne(a, slot, sys));
    return out;
}

} // namespace QodeAssist::Providers
