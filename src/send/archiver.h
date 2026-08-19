/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QStringList>

#include <atomic>
#include <memory>

namespace Tailshare
{

/**
 * Flag a caller raises to ask a running compression to stop.
 *
 * Shared with whichever thread is compressing, hence the shared_ptr: the job
 * that owns the flag may be destroyed while the worker is still running.
 */
using CancelFlag = std::shared_ptr<std::atomic_bool>;

/** Creates one, already raised or not. */
CancelFlag makeCancelFlag();

/**
 * Packs a selection into a single ZIP.
 *
 * Blocking: it is meant to run in a worker thread (see SendJob), never in the
 * file manager's own thread. KArchive offers no progress or interruption hook,
 * so cancellation is only honoured between top-level entries — a single huge
 * file or folder still has to finish before the flag is noticed.
 */
namespace Archiver
{

struct Result {
    bool ok = false;
    /** Failure in English, for logs and for the error notification. */
    QString error;
    /** True when the run stopped because the cancel flag was raised. */
    bool canceled = false;
};

/**
 * @param paths files and folders to pack, absolute.
 * @param archivePath where to write the ZIP; its directory must already exist.
 * @param cancel optional flag polled between entries.
 */
Result createZip(const QStringList &paths, const QString &archivePath, const CancelFlag &cancel = CancelFlag());

}
}
