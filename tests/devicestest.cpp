/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QFile>
#include <QTest>

#include "devices.h"
#include "statusparser.h"

using namespace Tailshare;

static DeviceList fixtureDevices(const QString &name)
{
    QFile file(QStringLiteral(FIXTURE_DIR "/") + name);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("could not open fixture %s", qPrintable(name));
        return DeviceList();
    }
    return parseStatus(file.readAll()).devices;
}

static QStringList displayNames(const DeviceList &devices)
{
    QStringList names;
    names.reserve(devices.size());
    for (const Device &device : devices) {
        names.append(device.displayName());
    }
    return names;
}

static Device makeDevice(const QString &hostName, bool online, TaildropTarget target)
{
    Device device;
    device.hostName = hostName;
    device.dnsName = hostName.toLower() + QStringLiteral(".tail1234.ts.net.");
    device.os = QStringLiteral("linux");
    device.online = online;
    device.taildropTarget = target;
    return device;
}

class DevicesTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void keepsOnlyDevicesThatCanReceive()
    {
        const DeviceList eligible = Devices::eligible(fixtureDevices(QStringLiteral("running-tailnet.json")));

        QCOMPARE(displayNames(eligible), QStringList({QStringLiteral("home-nas"), QStringLiteral("iphone-9")}));
    }

    void putsReceivableDevicesFirstThenSortsByName()
    {
        const DeviceList sorted = Devices::sorted(fixtureDevices(QStringLiteral("running-tailnet.json")));

        // home-nas and iphone-9 can receive; Aurora (no Taildrop) and pc-casa
        // (offline) sink to the bottom, still alphabetically among themselves.
        QCOMPARE(displayNames(sorted),
                 QStringList({QStringLiteral("home-nas"), QStringLiteral("iphone-9"), QStringLiteral("Aurora"), QStringLiteral("pc-casa")}));
    }

    void sortsCaseInsensitively()
    {
        const DeviceList devices{
            makeDevice(QStringLiteral("zebra"), true, TaildropTarget::Available),
            makeDevice(QStringLiteral("Alpha"), true, TaildropTarget::Available),
            makeDevice(QStringLiteral("beta"), true, TaildropTarget::Available),
        };

        QCOMPARE(displayNames(Devices::sorted(devices)),
                 QStringList({QStringLiteral("Alpha"), QStringLiteral("beta"), QStringLiteral("zebra")}));
    }

    void onlineFlagAloneDoesNotMakeADeviceEligible()
    {
        // tailscale reports peers that are online but still refuse Taildrop,
        // and the enum is the only thing that settles it.
        const DeviceList devices{
            makeDevice(QStringLiteral("online-no-taildrop"), true, TaildropTarget::MissingCap),
            makeDevice(QStringLiteral("offline-available"), false, TaildropTarget::Available),
        };

        QCOMPARE(displayNames(Devices::eligible(devices)), QStringList({QStringLiteral("offline-available")}));
    }

    void handlesEmptyList()
    {
        QVERIFY(Devices::eligible(DeviceList()).isEmpty());
        QVERIFY(Devices::sorted(DeviceList()).isEmpty());
    }

    void keepsIneligibleDevicesInTheMenu()
    {
        const DeviceList devices = fixtureDevices(QStringLiteral("running-tailnet.json"));

        // Sorting reorders, it never hides: the disabled items carry the reason.
        QCOMPARE(Devices::sorted(devices).size(), devices.size());
    }
};

QTEST_GUILESS_MAIN(DevicesTest)

#include "devicestest.moc"
