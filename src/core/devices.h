/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "device.h"

namespace Tailshare
{
/** Filtering and ordering rules for the device submenu. */
namespace Devices
{

/** The devices Taildrop will actually accept a transfer for. */
DeviceList eligible(const DeviceList &devices);

/**
 * Menu order: devices that can receive first, then alphabetically by display
 * name using the user's locale. Ineligible devices stay in the menu (disabled),
 * they only sink to the bottom.
 */
DeviceList sorted(const DeviceList &devices);

}
}
