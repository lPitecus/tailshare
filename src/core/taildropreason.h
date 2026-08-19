/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

#include "device.h"

namespace Tailshare
{

/**
 * Translated explanation for a disabled device, derived from the enum only.
 *
 * The backend's own @c NoFileSharingReason text is deliberately ignored: it is
 * untranslated, it changes between tailscale releases, and it sometimes leaks
 * internal wording. Returns an empty string when the device can receive files,
 * since there is nothing to explain.
 */
QString taildropReasonText(TaildropTarget target);

/** Convenience overload for a whole device. */
QString taildropReasonText(const Device &device);

}
