/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KAbstractFileItemActionPlugin>

#include <QVariantList>

#include "device.h"

/**
 * The "Share via Tailscale" submenu in Dolphin's context menu.
 *
 * Everything here runs inside the file manager's own thread while the menu is
 * being built, so it stays cheap and bounded: one @c "tailscale status --json"
 * with a hard timeout, no disk access, no network of our own. The moment a
 * device is picked the work moves to a SendJob and this class is out of the way.
 *
 * KIO caches and reuses the plugin instance, so no state survives between
 * calls: every menu is built from scratch out of a fresh status.
 */
class TailshareItemAction : public KAbstractFileItemActionPlugin
{
    Q_OBJECT

public:
    TailshareItemAction(QObject *parent, const QVariantList &args);

    QList<QAction *> actions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget) override;

private:
    /** Starts a transfer of @p paths to @p device, reporting through notifications. */
    void send(const QStringList &paths, const Tailshare::Device &device, QWidget *parentWidget);
};
