/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sendjob.h"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QtConcurrentRun>

#include "tailscaleclient.h"

using namespace Tailshare;

namespace
{

/**
 * Where the temporary ZIP goes.
 *
 * $XDG_RUNTIME_DIR is preferred: it is per user, mode 0700, and cleaned by the
 * session, so a transfer that dies badly cannot leave a copy of the user's
 * files behind in a world-readable /tmp.
 */
QString workDirTemplate()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (base.isEmpty() || !QFileInfo(base).isWritable()) {
        base = QDir::tempPath();
    }
    return base + QStringLiteral("/tailshare-XXXXXX");
}

}

SendJob::SendJob(const SendPlan &plan, const Device &target, QObject *parent)
    : QObject(parent)
    , m_plan(plan)
    , m_target(target)
    , m_program(TailscaleClient::findExecutable())
{
}

SendJob::~SendJob()
{
    // The worker thread may still be inside KArchive; raising the flag is all
    // we can do, and the shared_ptr keeps it alive for as long as it is needed.
    if (m_cancelFlag) {
        m_cancelFlag->store(true);
    }
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(200);
    }
}

QString SendJob::program() const
{
    return m_program;
}

void SendJob::setProgram(const QString &program)
{
    m_program = program;
}

int SendJob::timeout() const
{
    return m_timeout;
}

void SendJob::setTimeout(int milliseconds)
{
    m_timeout = qMax(0, milliseconds);
}

SendJob::State SendJob::state() const
{
    return m_state;
}

bool SendJob::isFinished() const
{
    return m_state == State::Succeeded || m_state == State::Failed || m_state == State::Canceled;
}

QString SendJob::errorText() const
{
    return m_error;
}

const SendPlan &SendJob::plan() const
{
    return m_plan;
}

const Device &SendJob::target() const
{
    return m_target;
}

int SendJob::itemCount() const
{
    return m_plan.sourcePaths().size();
}

void SendJob::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    Q_EMIT stateChanged(m_state);
    if (isFinished()) {
        Q_EMIT finished(m_state == State::Succeeded);
    }
}

void SendJob::fail(const QString &error)
{
    m_error = error;
    setState(State::Failed);
}

void SendJob::start()
{
    if (m_started || isFinished()) {
        return;
    }
    m_started = true;

    if (!m_plan.isValid()) {
        fail(m_plan.error());
        return;
    }
    if (m_program.isEmpty()) {
        fail(QStringLiteral("tailscale was not found in PATH"));
        return;
    }
    if (m_cancelRequested) {
        setState(State::Canceled);
        return;
    }

    if (m_plan.needsArchive()) {
        startCompression();
    } else {
        startTransfer(QString());
    }
}

void SendJob::startCompression()
{
    m_workDir = std::make_unique<QTemporaryDir>(workDirTemplate());
    if (!m_workDir->isValid()) {
        fail(QStringLiteral("could not create a temporary directory: %1").arg(m_workDir->errorString()));
        return;
    }

    const QString archivePath = m_workDir->filePath(m_plan.archiveFileName());
    m_cancelFlag = makeCancelFlag();

    auto *watcher = new QFutureWatcher<Archiver::Result>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, archivePath] {
        const Archiver::Result result = watcher->result();
        watcher->deleteLater();

        if (result.canceled || m_cancelRequested) {
            setState(State::Canceled);
        } else if (!result.ok) {
            fail(result.error);
        } else {
            startTransfer(archivePath);
        }
    });

    setState(State::Compressing);
    watcher->setFuture(QtConcurrent::run(&Archiver::createZip, m_plan.sourcePaths(), archivePath, m_cancelFlag));
}

void SendJob::startTransfer(const QString &archivePath)
{
    const QStringList arguments = m_plan.commandArguments(archivePath);
    if (arguments.isEmpty()) {
        fail(QStringLiteral("nothing to send: the plan produced no command"));
        return;
    }

    m_process = new QProcess(this);
    m_process->setProgram(m_program);
    m_process->setArguments(arguments);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::finished, this, &SendJob::onTransferFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            fail(QStringLiteral("could not run %1: %2").arg(m_program, m_process->errorString()));
        }
    });

    if (m_timeout > 0) {
        m_timer = new QTimer(this);
        m_timer->setSingleShot(true);
        connect(m_timer, &QTimer::timeout, this, [this] {
            m_timedOut = true;
            m_process->kill();
        });
        m_timer->start(m_timeout);
    }

    setState(State::Sending);
    m_process->start(QIODevice::ReadOnly);
}

void SendJob::onTransferFinished()
{
    if (m_timer) {
        m_timer->stop();
    }

    if (m_timedOut) {
        fail(QStringLiteral("tailscale did not finish within %1 ms").arg(m_timeout));
        return;
    }
    if (m_cancelRequested) {
        setState(State::Canceled);
        return;
    }

    if (m_process->exitStatus() != QProcess::NormalExit) {
        fail(QStringLiteral("tailscale crashed"));
        return;
    }
    if (m_process->exitCode() != 0) {
        // The CLI's own message is the useful one here: permission denied,
        // unknown peer, sharing disabled. Pass it through untouched.
        const QString stderrText = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        fail(stderrText.isEmpty() ? QStringLiteral("tailscale exited with %1").arg(m_process->exitCode()) : stderrText);
        return;
    }

    setState(State::Succeeded);
}

void SendJob::cancel()
{
    if (isFinished() || m_cancelRequested) {
        return;
    }
    m_cancelRequested = true;

    if (m_cancelFlag) {
        m_cancelFlag->store(true);
    }

    switch (m_state) {
    case State::Sending:
        // terminate() first so tailscale can clean up its side of the transfer.
        m_process->terminate();
        QTimer::singleShot(2000, this, [this] {
            if (m_process && m_process->state() != QProcess::NotRunning) {
                m_process->kill();
            }
        });
        break;
    case State::Idle:
        setState(State::Canceled);
        break;
    case State::Compressing:
        // The worker answers through the future watcher.
        break;
    default:
        break;
    }
}
