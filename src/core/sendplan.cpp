/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sendplan.h"

#include <QDir>
#include <QFileInfo>

using namespace Tailshare;

namespace
{

/**
 * Name of the single ZIP.
 *
 * One folder on its own keeps its own name; anything else is named after the
 * folder that holds the selection, which is what the user sees in Dolphin. When
 * that folder has no usable name (the file system root), a timestamp keeps the
 * archive recognisable on the receiving side.
 */
QString archiveNameFor(const QStringList &paths, const QDateTime &now)
{
    if (paths.size() == 1) {
        const QFileInfo info(paths.first());
        if (info.isDir() && !info.fileName().isEmpty()) {
            return info.fileName() + QStringLiteral(".zip");
        }
    }

    const QString parentName = QDir(QFileInfo(paths.first()).absolutePath()).dirName();
    if (!parentName.isEmpty()) {
        return parentName + QStringLiteral(".zip");
    }

    return QStringLiteral("tailshare-%1.zip").arg(now.toString(QStringLiteral("yyyyMMdd-HHmmss")));
}

}

SendPlan SendPlan::build(const QStringList &paths, const Device &target, const QDateTime &now)
{
    SendPlan plan;

    if (paths.isEmpty()) {
        plan.m_error = QStringLiteral("nothing to send: empty selection");
        return plan;
    }

    plan.m_targetArgument = target.sendTarget();
    if (plan.m_targetArgument.isEmpty()) {
        plan.m_error = QStringLiteral("no target: device has no DNS name");
        return plan;
    }

    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (!info.exists()) {
            plan.m_error = QStringLiteral("cannot send %1: no such file or directory").arg(path);
            plan.m_sourcePaths.clear();
            return plan;
        }
        plan.m_sourcePaths.append(info.absoluteFilePath());
        plan.m_needsArchive = plan.m_needsArchive || info.isDir();
    }

    if (plan.m_needsArchive) {
        plan.m_archiveFileName = archiveNameFor(plan.m_sourcePaths, now);
    }

    plan.m_valid = true;
    return plan;
}

bool SendPlan::isValid() const
{
    return m_valid;
}

QString SendPlan::error() const
{
    return m_error;
}

QStringList SendPlan::sourcePaths() const
{
    return m_sourcePaths;
}

bool SendPlan::needsArchive() const
{
    return m_needsArchive;
}

QString SendPlan::archiveFileName() const
{
    return m_archiveFileName;
}

QString SendPlan::targetArgument() const
{
    return m_targetArgument;
}

QStringList SendPlan::filesToSend(const QString &archivePath) const
{
    if (!m_valid) {
        return QStringList();
    }
    if (m_needsArchive) {
        return archivePath.isEmpty() ? QStringList() : QStringList{archivePath};
    }
    return m_sourcePaths;
}

QStringList SendPlan::commandArguments(const QString &archivePath) const
{
    const QStringList files = filesToSend(archivePath);
    if (files.isEmpty()) {
        return QStringList();
    }

    // "--" stops flag parsing, so a file named "-v" is still a file.
    // "--name" is deliberately absent: it is incompatible with several files
    // (cmd/tailscale/cli/file.go) and the ZIP is already named the way we want.
    QStringList arguments{QStringLiteral("file"), QStringLiteral("cp"), QStringLiteral("--")};
    arguments.append(files);
    arguments.append(m_targetArgument);
    return arguments;
}
