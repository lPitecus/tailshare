/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tailscaleclient.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

using namespace Tailshare;

TailscaleClient::TailscaleClient()
    : m_program(findExecutable())
{
}

QString TailscaleClient::findExecutable()
{
    // An explicit path wins over the PATH lookup. It is what lets the tests
    // drive the plugin with a scripted tailnet, and it is the escape hatch for
    // an installation that keeps the binary somewhere unusual.
    //
    // It has to be absolute. A bare name would be resolved against the working
    // directory of whichever process loaded the plugin — for Dolphin, whatever
    // folder it happens to be sitting in — and running a binary picked up from
    // there is never what this hatch is for. Note this is not a defence against
    // a hostile environment: $PATH already decides which tailscale runs.
    const QString override = qEnvironmentVariable("TAILSHARE_TAILSCALE");
    if (!override.isEmpty()) {
        return QFileInfo(override).isAbsolute() ? override : QString();
    }

    return QStandardPaths::findExecutable(QStringLiteral("tailscale"));
}

QString TailscaleClient::program() const
{
    return m_program;
}

void TailscaleClient::setProgram(const QString &program)
{
    m_program = program;
}

QStringList TailscaleClient::arguments() const
{
    return m_arguments;
}

void TailscaleClient::setArguments(const QStringList &arguments)
{
    m_arguments = arguments;
}

int TailscaleClient::timeout() const
{
    return m_timeout;
}

void TailscaleClient::setTimeout(int milliseconds)
{
    m_timeout = milliseconds;
}

Status TailscaleClient::fetchStatus() const
{
    Status status;

    if (m_program.isEmpty()) {
        status.error = qEnvironmentVariableIsSet("TAILSHARE_TAILSCALE") ? QStringLiteral("$TAILSHARE_TAILSCALE must be an absolute path")
                                                                        : QStringLiteral("tailscale was not found in PATH");
        return status;
    }

    QElapsedTimer clock;
    clock.start();
    const auto remaining = [this, &clock] {
        return qMax(0, m_timeout - static_cast<int>(clock.elapsed()));
    };

    QProcess process;
    process.setProgram(m_program);
    process.setArguments(m_arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(QIODevice::ReadOnly);

    if (!process.waitForStarted(remaining())) {
        process.kill();
        process.waitForFinished(100);
        status.error = QStringLiteral("could not run %1: %2").arg(m_program, process.errorString());
        return status;
    }

    if (!process.waitForFinished(remaining())) {
        // Whatever tailscaled is doing, the context menu cannot wait for it.
        process.kill();
        process.waitForFinished(100);
        status.error = QStringLiteral("tailscale did not answer within %1 ms").arg(m_timeout);
        return status;
    }

    if (process.exitStatus() != QProcess::NormalExit) {
        status.error = QStringLiteral("tailscale crashed");
        return status;
    }

    if (process.exitCode() != 0) {
        const QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
        status.error = QStringLiteral("tailscale exited with %1: %2").arg(QString::number(process.exitCode()), stderrText);
        return status;
    }

    return parseStatus(process.readAllStandardOutput());
}
