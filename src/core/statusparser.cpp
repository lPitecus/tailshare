/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "statusparser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

using namespace Tailshare;

bool Status::isRunning() const
{
    return backendState == QLatin1String("Running");
}

static Device deviceFromJson(const QJsonObject &peer)
{
    Device device;
    device.hostName = peer.value(QLatin1String("HostName")).toString();
    device.dnsName = peer.value(QLatin1String("DNSName")).toString();
    device.os = peer.value(QLatin1String("OS")).toString();
    device.online = peer.value(QLatin1String("Online")).toBool(false);
    device.taildropTarget = taildropTargetFromValue(peer.value(QLatin1String("TaildropTarget")).toInt(0));
    device.noFileSharingReason = peer.value(QLatin1String("NoFileSharingReason")).toString();
    return device;
}

Status Tailshare::parseStatus(const QByteArray &json)
{
    Status status;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        status.error = QStringLiteral("malformed JSON: %1").arg(parseError.errorString());
        return status;
    }
    if (!document.isObject()) {
        status.error = QStringLiteral("unexpected JSON: top level is not an object");
        return status;
    }

    const QJsonObject root = document.object();
    status.valid = true;
    status.backendState = root.value(QLatin1String("BackendState")).toString();

    // "Peer" is null on a tailnet where this machine is the only device.
    const QJsonObject peers = root.value(QLatin1String("Peer")).toObject();
    for (auto it = peers.constBegin(); it != peers.constEnd(); ++it) {
        if (!it.value().isObject()) {
            continue;
        }
        const Device device = deviceFromJson(it.value().toObject());
        // A peer we cannot address is useless to us, whatever else it says.
        if (device.dnsName.isEmpty()) {
            continue;
        }
        status.devices.append(device);
    }

    return status;
}
