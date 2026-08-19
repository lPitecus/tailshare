/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "archiver.h"

#include <KZip>

#include <QFileInfo>
#include <QSet>

using namespace Tailshare;

namespace
{

/**
 * Entry name inside the archive, unique within it.
 *
 * A selection can hold two items with the same name coming from different
 * folders (Dolphin allows it through search results and multi-tab selection);
 * writing both under one name would silently drop one of them.
 */
QString uniqueEntryName(const QFileInfo &info, QSet<QString> &taken)
{
    const QString name = info.fileName();
    if (!taken.contains(name)) {
        taken.insert(name);
        return name;
    }

    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();
    for (int n = 2; n < 1000; ++n) {
        QString candidate = suffix.isEmpty() ? QStringLiteral("%1-%2").arg(base, QString::number(n))
                                             : QStringLiteral("%1-%2.%3").arg(base, QString::number(n), suffix);
        if (!taken.contains(candidate)) {
            taken.insert(candidate);
            return candidate;
        }
    }

    // A selection with a thousand identical names is not a real case; refuse
    // rather than loop forever.
    return QString();
}

bool isCanceled(const CancelFlag &cancel)
{
    return cancel && cancel->load();
}

}

CancelFlag Tailshare::makeCancelFlag()
{
    return std::make_shared<std::atomic_bool>(false);
}

Archiver::Result Archiver::createZip(const QStringList &paths, const QString &archivePath, const CancelFlag &cancel)
{
    Result result;

    if (paths.isEmpty()) {
        result.error = QStringLiteral("nothing to compress: empty selection");
        return result;
    }
    if (archivePath.isEmpty()) {
        result.error = QStringLiteral("no archive path given");
        return result;
    }

    KZip zip(archivePath);
    if (!zip.open(QIODevice::WriteOnly)) {
        result.error = QStringLiteral("could not create %1: %2").arg(archivePath, zip.errorString());
        return result;
    }

    QSet<QString> taken;
    for (const QString &path : paths) {
        if (isCanceled(cancel)) {
            zip.close();
            result.canceled = true;
            return result;
        }

        const QFileInfo info(path);
        const QString entryName = uniqueEntryName(info, taken);
        if (entryName.isEmpty()) {
            zip.close();
            result.error = QStringLiteral("too many items named %1").arg(info.fileName());
            return result;
        }

        const bool added = info.isDir() ? zip.addLocalDirectory(path, entryName) : zip.addLocalFile(path, entryName);
        if (!added) {
            const QString reason = zip.errorString();
            zip.close();
            result.error = reason.isEmpty() ? QStringLiteral("could not add %1 to the archive").arg(path)
                                            : QStringLiteral("could not add %1: %2").arg(path, reason);
            return result;
        }
    }

    // close() is where the central directory is written: a failure here means
    // the ZIP on disk is unusable, however well the entries went.
    if (!zip.close()) {
        result.error = QStringLiteral("could not finish %1: %2").arg(archivePath, zip.errorString());
        return result;
    }

    if (isCanceled(cancel)) {
        result.canceled = true;
        return result;
    }

    result.ok = true;
    return result;
}
