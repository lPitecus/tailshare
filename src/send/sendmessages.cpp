/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sendmessages.h"

#include <KLocalizedString>

using namespace Tailshare;

QString SendMessages::title(const SendJob &job)
{
    switch (job.state()) {
    case SendJob::State::Compressing:
        return i18nc("@title notification", "Compressing");
    case SendJob::State::Sending:
        return i18nc("@title notification", "Sending files");
    case SendJob::State::Succeeded:
        return i18nc("@title notification", "Files sent");
    case SendJob::State::Failed:
        return i18nc("@title notification", "Send failed");
    case SendJob::State::Canceled:
        return i18nc("@title notification", "Send canceled");
    case SendJob::State::Idle:
        break;
    }
    return QString();
}

QString SendMessages::text(const SendJob &job)
{
    const QString device = job.target().displayName();
    const int count = job.itemCount();

    switch (job.state()) {
    case SendJob::State::Compressing:
        // The folder is what makes the ZIP necessary, so it is what the user
        // is told about; the archive name is the same one they will receive.
        return i18ncp("@info notification, %2 is a device name",
                      "Packing %1 item into %3 for %2",
                      "Packing %1 items into %3 for %2",
                      count,
                      device,
                      job.plan().archiveFileName());
    case SendJob::State::Sending:
        if (job.plan().needsArchive()) {
            return i18nc("@info notification, %1 is a file name, %2 a device name", "Sending %1 to %2", job.plan().archiveFileName(), device);
        }
        return i18ncp("@info notification, %2 is a device name", "Sending %1 file to %2", "Sending %1 files to %2", count, device);
    case SendJob::State::Succeeded:
        if (job.plan().needsArchive()) {
            return i18nc("@info notification, %1 is a file name, %2 a device name", "Sent %1 to %2", job.plan().archiveFileName(), device);
        }
        return i18ncp("@info notification, %2 is a device name", "Sent %1 file to %2", "Sent %1 files to %2", count, device);
    case SendJob::State::Failed:
        // tailscale's own wording, which is the one that says what went wrong.
        return job.errorText();
    case SendJob::State::Canceled:
        return i18nc("@info notification, %1 is a device name", "The transfer to %1 was canceled.", device);
    case SendJob::State::Idle:
        break;
    }
    return QString();
}

QString SendMessages::iconName(const SendJob &job)
{
    switch (job.state()) {
    case SendJob::State::Compressing:
        return QStringLiteral("archive-insert-directory");
    case SendJob::State::Sending:
        return QStringLiteral("document-send");
    case SendJob::State::Succeeded:
        return QStringLiteral("task-complete");
    case SendJob::State::Failed:
        return QStringLiteral("dialog-error");
    case SendJob::State::Canceled:
        return QStringLiteral("dialog-cancel");
    case SendJob::State::Idle:
        break;
    }
    return QString();
}

QString SendMessages::eventId(SendJob::State state)
{
    switch (state) {
    case SendJob::State::Compressing:
    case SendJob::State::Sending:
        return QStringLiteral("sending");
    case SendJob::State::Succeeded:
        return QStringLiteral("sent");
    case SendJob::State::Failed:
    case SendJob::State::Canceled:
        return QStringLiteral("error");
    case SendJob::State::Idle:
        break;
    }
    return QString();
}
