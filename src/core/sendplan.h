/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

#include "device.h"

namespace Tailshare
{

/**
 * What to send and how, decided before anything touches the disk or the network.
 *
 * @c "tailscale file cp" refuses directories outright (cmd/tailscale/cli/file.go),
 * so a selection containing any folder is compressed into a single ZIP first. A
 * selection of plain files is sent as it is, in one command.
 *
 * The class is pure except for the @c QFileInfo lookups it needs to tell files
 * from folders, which keeps it testable without a tailnet.
 */
class SendPlan
{
public:
    /**
     * @param paths local file system paths, as handed over by the file manager.
     * @param target the device that should receive them.
     * @param now clock used for the fallback archive name; injectable for tests.
     */
    static SendPlan build(const QStringList &paths, const Device &target, const QDateTime &now = QDateTime::currentDateTime());

    bool isValid() const;
    /** Why the plan could not be built, in English, for logs. */
    QString error() const;

    /** The selection, absolute and cleaned, in the order it was given. */
    QStringList sourcePaths() const;

    /** True when the selection holds at least one folder and must be zipped. */
    bool needsArchive() const;
    /** File name of the ZIP to create, empty when no archive is needed. */
    QString archiveFileName() const;

    /** The @c "<name>:" argument of the receiving device. */
    QString targetArgument() const;

    /**
     * The files @c "tailscale file cp" should be given.
     * @param archivePath where the ZIP was written; ignored when no archive is needed.
     */
    QStringList filesToSend(const QString &archivePath = QString()) const;

    /**
     * Full argument list for the @c tailscale executable, ready for QProcess.
     * Never a shell string: spaces and quotes in file names stay safe by construction.
     */
    QStringList commandArguments(const QString &archivePath = QString()) const;

private:
    bool m_valid = false;
    QString m_error;
    QStringList m_sourcePaths;
    bool m_needsArchive = false;
    QString m_archiveFileName;
    QString m_targetArgument;
};

}
