/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "device.h"

using namespace Tailshare;

TaildropTarget Tailshare::taildropTargetFromValue(int value)
{
    switch (value) {
    case 0:
        return TaildropTarget::Unknown;
    case 1:
        return TaildropTarget::Available;
    case 2:
        return TaildropTarget::NoNetmapAvailable;
    case 3:
        return TaildropTarget::IpnStateNotRunning;
    case 4:
        return TaildropTarget::MissingCap;
    case 5:
        return TaildropTarget::Offline;
    case 6:
        return TaildropTarget::NoPeerInfo;
    case 7:
        return TaildropTarget::UnsupportedOS;
    case 8:
        return TaildropTarget::NoPeerAPI;
    case 9:
        return TaildropTarget::OwnedByOtherUser;
    default:
        // A newer tailscale may grow the enum; an unknown reason is still a reason.
        return TaildropTarget::Unknown;
    }
}

QString Device::displayName() const
{
    const QString trimmed = hostName.trimmed();
    if (!trimmed.isEmpty() && trimmed.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) != 0) {
        return trimmed;
    }

    const QString firstLabel = dnsName.section(QLatin1Char('.'), 0, 0);
    if (!firstLabel.isEmpty()) {
        return firstLabel;
    }

    return trimmed;
}

QString Device::sendTarget() const
{
    QString name = dnsName.trimmed();
    while (name.endsWith(QLatin1Char('.'))) {
        name.chop(1);
    }
    if (name.isEmpty()) {
        return QString();
    }
    return name + QLatin1Char(':');
}

bool Device::canReceiveFiles() const
{
    return taildropTarget == TaildropTarget::Available && !sendTarget().isEmpty();
}
