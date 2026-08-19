/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tailshareitemaction.h"

#include <KFileItem>
#include <KFileItemListProperties>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <QMenu>

#include "closeguard.h"
#include "devices.h"
#include "sendjob.h"
#include "sendnotifier.h"
#include "sendplan.h"
#include "taildropreason.h"
#include "tailscaleclient.h"

using namespace Tailshare;

namespace
{

/**
 * Theme icon for a peer, from the OS tailscale reports.
 *
 * Only Breeze names that exist across themes, and nothing branded: the plan
 * keeps trademarks out of the menu.
 */
QString iconForOs(const QString &os)
{
    const QString lower = os.toLower();
    if (lower == QLatin1String("ios") || lower == QLatin1String("android")) {
        return QStringLiteral("smartphone");
    }
    if (lower == QLatin1String("macos")) {
        return QStringLiteral("computer-laptop");
    }
    return QStringLiteral("computer");
}

QStringList localPaths(const KFileItemListProperties &properties)
{
    QStringList paths;
    const KFileItemList items = properties.items();
    paths.reserve(items.size());
    for (const KFileItem &item : items) {
        const QString path = item.url().toLocalFile();
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }
    return paths;
}

}

TailshareItemAction::TailshareItemAction(QObject *parent, const QVariantList &)
    : KAbstractFileItemActionPlugin(parent)
{
}

QList<QAction *> TailshareItemAction::actions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget)
{
    // Remote selections would have to be downloaded first; that is v2.
    if (!fileItemInfos.isLocal()) {
        return {};
    }

    const QStringList paths = localPaths(fileItemInfos);
    if (paths.isEmpty()) {
        return {};
    }

    // Nothing below may block: a slow answer here delays Dolphin's whole
    // context menu, so the client gives up after its timeout and we vanish.
    TailscaleClient client;
    if (client.program().isEmpty()) {
        return {};
    }

    const Status status = client.fetchStatus();
    if (!status.valid || !status.isRunning()) {
        return {};
    }

    const DeviceList devices = Devices::sorted(status.devices);
    if (devices.isEmpty()) {
        return {};
    }

    auto *menuAction = new QAction(QIcon::fromTheme(QStringLiteral("document-send")), i18nc("@action:inmenu", "Share via Tailscale"), parentWidget);
    auto *menu = new QMenu(parentWidget);
    // Ineligible devices explain themselves in a tooltip, which a menu only
    // shows when asked to.
    menu->setToolTipsVisible(true);
    menuAction->setMenu(menu);
    // KIO owns the action; the menu should not outlive it.
    connect(menuAction, &QObject::destroyed, menu, &QObject::deleteLater);

    for (const Device &device : devices) {
        QAction *action = menu->addAction(QIcon::fromTheme(iconForOs(device.os)), device.displayName());
        if (!device.canReceiveFiles()) {
            action->setEnabled(false);
            action->setToolTip(taildropReasonText(device));
            continue;
        }
        connect(action, &QAction::triggered, this, [this, paths, device, parentWidget] {
            send(paths, device, parentWidget);
        });
    }

    return {menuAction};
}

void TailshareItemAction::send(const QStringList &paths, const Device &device, QWidget *parentWidget)
{
    // Parented to the application, not to this plugin or to the menu: both are
    // gone moments after the click, and the transfer has to outlive them.
    auto *job = new SendJob(SendPlan::build(paths, device), device, qApp);
    new SendNotifier(job, job);
    CloseGuard::watch(parentWidget, job);
    connect(job, &SendJob::finished, job, &QObject::deleteLater);

    job->start();
}

K_PLUGIN_CLASS_WITH_JSON(TailshareItemAction, "tailshareitemaction.json")

#include "tailshareitemaction.moc"
