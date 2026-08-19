/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QString>

#include <memory>

#include "archiver.h"
#include "device.h"
#include "sendplan.h"

class QProcess;
class QTemporaryDir;
class QTimer;

namespace Tailshare
{

/**
 * One transfer, from the click to the notification, without ever blocking.
 *
 * Lives in the caller's thread and returns immediately from start(): the
 * compression runs on a worker thread and @c "tailscale file cp" runs as an
 * asynchronous QProcess, so the file manager stays responsive throughout. All
 * progress is reported by signals; this class draws nothing and knows nothing
 * about notifications (see SendNotifier).
 *
 * The temporary ZIP is written under a QTemporaryDir that is removed when the
 * job dies, whatever the outcome.
 */
class SendJob : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        Compressing,
        Sending,
        Succeeded,
        Failed,
        Canceled,
    };
    Q_ENUM(State)

    SendJob(const SendPlan &plan, const Device &target, QObject *parent = nullptr);
    ~SendJob() override;

    /** The tailscale executable; defaults to the one found in PATH. */
    QString program() const;
    void setProgram(const QString &program);

    /**
     * Optional ceiling for the transfer itself, in milliseconds; 0 (the
     * default) means no ceiling.
     *
     * Deliberately off: a legitimate multi-gigabyte transfer and a send hung on
     * a peer that went offline look identical from here, and killing the first
     * to catch the second is the worse trade. cancel() is the way out, and the
     * menu already refuses to offer devices Taildrop reports as unreachable.
     */
    int timeout() const;
    void setTimeout(int milliseconds);

    State state() const;
    bool isFinished() const;
    /** Empty unless the state is Failed. */
    QString errorText() const;

    const SendPlan &plan() const;
    const Device &target() const;
    /** How many items the user picked, before any compression. */
    int itemCount() const;

public Q_SLOTS:
    /** Starts the job; does nothing if it already ran. */
    void start();
    /**
     * Asks the job to stop as soon as it can.
     *
     * During compression the flag is only seen between top-level entries; during
     * the transfer the process is asked to terminate, then killed. Either way
     * the job still finishes through finished(), with state Canceled.
     */
    void cancel();

Q_SIGNALS:
    void stateChanged(Tailshare::SendJob::State state);
    /** Emitted exactly once, after the last stateChanged(). */
    void finished(bool ok);

private:
    void setState(State state);
    void fail(const QString &error);
    void startCompression();
    void startTransfer(const QString &archivePath);
    void onTransferFinished();

    SendPlan m_plan;
    Device m_target;
    QString m_program;
    int m_timeout = 0;

    State m_state = State::Idle;
    QString m_error;
    bool m_started = false;
    bool m_cancelRequested = false;
    bool m_timedOut = false;

    CancelFlag m_cancelFlag;
    std::unique_ptr<QTemporaryDir> m_workDir;
    QProcess *m_process = nullptr;
    QTimer *m_timer = nullptr;
};

}
