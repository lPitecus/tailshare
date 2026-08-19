/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QString>

#include "device.h"

namespace Tailshare
{

/** Result of one @c "tailscale status --json" call. */
class Status
{
public:
    /** False when the output could not be parsed at all. */
    bool valid = false;
    /** Parse or execution failure, in English, for logs. */
    QString error;
    /** Raw backend state: Running, NeedsLogin, Stopped, Starting, NoState... */
    QString backendState;
    /** Every peer of the tailnet, in the order the backend reported them. */
    DeviceList devices;

    bool isRunning() const;
};

/**
 * Parses the JSON of @c "tailscale status --json".
 *
 * Deliberately tolerant: the tailscale CLI warns that this format may change,
 * so a missing or mistyped field degrades to a safe default instead of failing.
 * Only output that is not a JSON object at all is rejected.
 */
Status parseStatus(const QByteArray &json);

}
