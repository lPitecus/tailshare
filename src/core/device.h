/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include <QString>

namespace Tailshare
{

/**
 * Why a peer can or cannot receive files over Taildrop.
 *
 * Mirrors tailscale's @c TaildropTargetStatus enum (ipn/ipnstate/ipnstate.go).
 * The numeric values are part of the @c "tailscale status --json" output and
 * must not be reordered.
 */
enum class TaildropTarget {
    Unknown = 0,
    Available = 1,
    NoNetmapAvailable = 2,
    IpnStateNotRunning = 3,
    MissingCap = 4,
    Offline = 5,
    NoPeerInfo = 6,
    UnsupportedOS = 7,
    NoPeerAPI = 8,
    OwnedByOtherUser = 9,
};

/** Maps a raw JSON value to the enum; anything unknown degrades to Unknown. */
TaildropTarget taildropTargetFromValue(int value);

/** One peer of the tailnet, reduced to what the menu and the send need. */
class Device
{
public:
    QString hostName;
    /** Fully qualified MagicDNS name, as reported (may end with a dot). */
    QString dnsName;
    /** Operating system as reported by tailscale: linux, windows, iOS, android, macOS. */
    QString os;
    bool online = false;
    TaildropTarget taildropTarget = TaildropTarget::Unknown;
    /** Raw backend text; kept for logs only, never shown to the user. */
    QString noFileSharingReason;

    /**
     * Name to show in the menu.
     *
     * Prefers the host name, but several real devices report a useless
     * @c "localhost" (iOS and Android do), so the first label of the MagicDNS
     * name is used whenever the host name carries no information.
     */
    QString displayName() const;

    /** The @c "<name>:" argument @c "tailscale file cp" expects, without the trailing dot. */
    QString sendTarget() const;

    /** Whether Taildrop will actually accept a transfer for this peer. */
    bool canReceiveFiles() const;
};

using DeviceList = QList<Device>;

}
