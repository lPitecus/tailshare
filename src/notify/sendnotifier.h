/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QPointer>

#include "sendjob.h"

class KNotification;
class QTimer;

namespace Tailshare
{

/**
 * Turns a SendJob into Plasma notifications.
 *
 * One persistent notification follows the transfer, carrying a Cancel action,
 * and is replaced at the end by a short-lived success or failure one. The
 * wording comes from SendMessages, so the probe and the plugin say the same
 * thing.
 *
 * The progress notification is only raised once the transfer has lasted longer
 * than a delay. Two reasons: sending a couple of small files takes a few
 * milliseconds, and a popup that appears and vanishes is pure noise; and
 * KNotification can only update a notification after the server has answered
 * with its id, so an update issued before that arrives lands *after* the final
 * notification and resurrects the progress popup (measured here, see PLAN.md
 * section 3.5).
 *
 * The component name must match the installed @c tailshare.notifyrc, which is
 * what gives the events their user-visible names and default actions.
 */
class SendNotifier : public QObject
{
    Q_OBJECT

public:
    /** How long a transfer must last before a progress popup is worth it. */
    static constexpr int DefaultDelayMs = 400;

    /** Attaches to @p job; safe to create before or after start(). */
    explicit SendNotifier(SendJob *job, QObject *parent = nullptr);
    ~SendNotifier() override;

    int delay() const;
    void setDelay(int milliseconds);

    /** The notifyrc this notifier reads its events from. */
    static QString componentName();

private:
    void onStateChanged(SendJob::State state);
    void showProgress();
    void closeProgress();

    SendJob *m_job = nullptr;
    int m_delay = DefaultDelayMs;
    QTimer *m_delayTimer = nullptr;
    QPointer<KNotification> m_progress;
};

}
