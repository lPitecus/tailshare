/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "taildropreason.h"

#include <KLocalizedString>

using namespace Tailshare;

QString Tailshare::taildropReasonText(TaildropTarget target)
{
    switch (target) {
    case TaildropTarget::Available:
        return QString();
    case TaildropTarget::Offline:
        return i18n("This device is offline.");
    case TaildropTarget::MissingCap:
        return i18n("Taildrop is not enabled for this device.");
    case TaildropTarget::UnsupportedOS:
        return i18n("This device runs an operating system that cannot receive files.");
    case TaildropTarget::OwnedByOtherUser:
        return i18n("This device belongs to another user. Taildrop only works between your own devices.");
    case TaildropTarget::IpnStateNotRunning:
        return i18n("Tailscale is not running.");
    case TaildropTarget::NoNetmapAvailable:
    case TaildropTarget::NoPeerInfo:
    case TaildropTarget::NoPeerAPI:
        return i18n("Tailscale has no connection details for this device yet.");
    case TaildropTarget::Unknown:
        break;
    }

    return i18n("This device cannot receive files right now.");
}

QString Tailshare::taildropReasonText(const Device &device)
{
    return taildropReasonText(device.taildropTarget);
}
