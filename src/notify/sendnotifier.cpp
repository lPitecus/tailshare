/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sendnotifier.h"

#include <KLocalizedString>
#include <KNotification> // also declares KNotificationAction

#include <QTimer>

#include "sendmessages.h"

using namespace Tailshare;

SendNotifier::SendNotifier(SendJob *job, QObject *parent)
    : QObject(parent)
    , m_job(job)
    , m_delayTimer(new QTimer(this))
{
    Q_ASSERT(job);

    m_delayTimer->setSingleShot(true);
    connect(m_delayTimer, &QTimer::timeout, this, &SendNotifier::showProgress);

    connect(job, &SendJob::stateChanged, this, &SendNotifier::onStateChanged);
    if (job->state() != SendJob::State::Idle) {
        onStateChanged(job->state());
    }
}

SendNotifier::~SendNotifier()
{
    closeProgress();
}

int SendNotifier::delay() const
{
    return m_delay;
}

void SendNotifier::setDelay(int milliseconds)
{
    m_delay = qMax(0, milliseconds);
}

QString SendNotifier::componentName()
{
    return QStringLiteral("tailshare");
}

void SendNotifier::onStateChanged(SendJob::State state)
{
    if (state == SendJob::State::Idle) {
        return;
    }

    if (state == SendJob::State::Compressing || state == SendJob::State::Sending) {
        if (m_progress) {
            // Already on screen and already carrying an id: updating is safe.
            m_progress->setTitle(SendMessages::title(*m_job));
            m_progress->setText(SendMessages::text(*m_job));
            m_progress->setIconName(SendMessages::iconName(*m_job));
        } else if (!m_delayTimer->isActive()) {
            m_delayTimer->start(m_delay);
        }
        return;
    }

    // A transfer that never outlived the delay says nothing until it is over.
    m_delayTimer->stop();
    closeProgress();
    KNotification::event(SendMessages::eventId(state),
                         SendMessages::title(*m_job),
                         SendMessages::text(*m_job),
                         SendMessages::iconName(*m_job),
                         KNotification::CloseOnTimeout,
                         componentName());
}

void SendNotifier::showProgress()
{
    if (m_progress || m_job->isFinished()) {
        return;
    }

    m_progress = new KNotification(SendMessages::eventId(m_job->state()), KNotification::Persistent, this);
    m_progress->setComponentName(componentName());
    m_progress->setTitle(SendMessages::title(*m_job));
    m_progress->setText(SendMessages::text(*m_job));
    m_progress->setIconName(SendMessages::iconName(*m_job));

    auto *cancelAction = m_progress->addAction(i18nc("@action:button notification", "Cancel"));
    connect(cancelAction, &KNotificationAction::activated, m_job, &SendJob::cancel);

    m_progress->sendEvent();
}

void SendNotifier::closeProgress()
{
    if (m_progress) {
        m_progress->close();
        m_progress.clear();
    }
}
