/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "devices.h"

#include <QCollator>

#include <algorithm>

using namespace Tailshare;

DeviceList Devices::eligible(const DeviceList &devices)
{
    DeviceList result;
    result.reserve(devices.size());
    for (const Device &device : devices) {
        if (device.canReceiveFiles()) {
            result.append(device);
        }
    }
    return result;
}

DeviceList Devices::sorted(const DeviceList &devices)
{
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);

    DeviceList result = devices;
    std::stable_sort(result.begin(), result.end(), [&collator](const Device &lhs, const Device &rhs) {
        if (lhs.canReceiveFiles() != rhs.canReceiveFiles()) {
            return lhs.canReceiveFiles();
        }
        const int byName = collator.compare(lhs.displayName(), rhs.displayName());
        if (byName != 0) {
            return byName < 0;
        }
        // Two devices may share a display name; the DNS name never repeats.
        return lhs.dnsName < rhs.dnsName;
    });
    return result;
}
