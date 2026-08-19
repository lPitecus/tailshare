/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "closeguard.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QCloseEvent>
#include <QHash>
#include <QWidget>

#include "sendjob.h"

using namespace Tailshare;

namespace
{
/** One guard per window; the guard dies with the window it is parented to. */
QHash<QWidget *, CloseGuard *> &guards()
{
    static QHash<QWidget *, CloseGuard *> instances;
    return instances;
}

QWidget *topLevelOf(QWidget *widget)
{
    return widget ? widget->window() : nullptr;
}
}

CloseGuard::CloseGuard(QWidget *window)
    : QObject(window)
    , m_window(window)
{
    window->installEventFilter(this);
    connect(window, &QObject::destroyed, this, [window] {
        guards().remove(window);
    });
}

void CloseGuard::watch(QWidget *widget, SendJob *job)
{
    QWidget *window = topLevelOf(widget);
    if (!window || !job) {
        return;
    }

    CloseGuard *guard = guards().value(window);
    if (!guard) {
        guard = new CloseGuard(window);
        guards().insert(window, guard);
    }
    guard->add(job);
}

int CloseGuard::activeJobs(QWidget *widget)
{
    CloseGuard *guard = guards().value(topLevelOf(widget));
    return guard ? guard->pruneAndCount() : 0;
}

void CloseGuard::add(SendJob *job)
{
    m_jobs.append(QPointer<SendJob>(job));
}

int CloseGuard::pruneAndCount()
{
    int running = 0;
    for (auto it = m_jobs.begin(); it != m_jobs.end();) {
        if (it->isNull() || (*it)->isFinished()) {
            it = m_jobs.erase(it);
        } else {
            ++running;
            ++it;
        }
    }
    return running;
}

bool CloseGuard::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_window || event->type() != QEvent::Close) {
        return QObject::eventFilter(watched, event);
    }

    const int running = pruneAndCount();
    if (running == 0) {
        return QObject::eventFilter(watched, event);
    }

    const auto answer = KMessageBox::warningContinueCancel(
        m_window,
        i18ncp("@info", "A file is still being sent via Tailscale.", "%1 files are still being sent via Tailscale.", running),
        i18nc("@title:window", "Transfer in Progress"),
        KStandardGuiItem::cont(),
        KStandardGuiItem::cancel(),
        QStringLiteral("tailshare_close_with_transfer"));

    if (answer == KMessageBox::Continue) {
        return QObject::eventFilter(watched, event);
    }

    // The user chose the transfer over the window.
    event->ignore();
    return true;
}
