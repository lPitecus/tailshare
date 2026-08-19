/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QPointer>

class QWidget;

namespace Tailshare
{
class SendJob;

/**
 * Warns before a window is closed while it still has a transfer in flight.
 *
 * In v1 the transfer runs inside the file manager, so closing the window kills
 * it. There is no supported way for a context menu plugin to veto the host's
 * close, so this filters @c QEvent::Close on the top level window — knowingly
 * outside the API contract. It is confined to this one class and fails open:
 * if the event never arrives, or the filter is never reached, the close simply
 * proceeds as it always did, and the user loses the transfer rather than the
 * window.
 *
 * The real fix is the detached helper planned for v2.
 */
class CloseGuard : public QObject
{
    Q_OBJECT

public:
    /** Watches the window @p widget belongs to for as long as @p job runs. */
    static void watch(QWidget *widget, SendJob *job);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit CloseGuard(QWidget *window);

    void add(SendJob *job);
    int pruneAndCount();

    QWidget *m_window = nullptr;
    QList<QPointer<SendJob>> m_jobs;
};

}
