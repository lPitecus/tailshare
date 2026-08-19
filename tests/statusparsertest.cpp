/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QFile>
#include <QTest>

#include "statusparser.h"

using namespace Tailshare;

static QByteArray fixture(const QString &name)
{
    QFile file(QStringLiteral(FIXTURE_DIR "/") + name);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("could not open fixture %s", qPrintable(name));
        return QByteArray();
    }
    return file.readAll();
}

class StatusParserTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesRunningTailnet()
    {
        const Status status = parseStatus(fixture(QStringLiteral("running-tailnet.json")));

        QVERIFY(status.valid);
        QVERIFY(status.error.isEmpty());
        QCOMPARE(status.backendState, QStringLiteral("Running"));
        QVERIFY(status.isRunning());
        // Self is not a peer and must never show up as a send target.
        QCOMPARE(status.devices.size(), 4);

        const Device nas = status.devices.at(0);
        QCOMPARE(nas.hostName, QStringLiteral("home-nas"));
        QCOMPARE(nas.dnsName, QStringLiteral("home-nas.tail1234.ts.net."));
        QCOMPARE(nas.os, QStringLiteral("linux"));
        QVERIFY(nas.online);
        QCOMPARE(nas.taildropTarget, TaildropTarget::Available);
        QVERIFY(nas.canReceiveFiles());
        QCOMPARE(nas.sendTarget(), QStringLiteral("home-nas.tail1234.ts.net:"));
    }

    void usesDnsNameWhenHostNameIsGeneric()
    {
        const Status status = parseStatus(fixture(QStringLiteral("running-tailnet.json")));
        const Device phone = status.devices.at(1);

        QCOMPARE(phone.hostName, QStringLiteral("localhost"));
        QCOMPARE(phone.displayName(), QStringLiteral("iphone-9"));
    }

    void readsOfflinePeer()
    {
        const Status status = parseStatus(fixture(QStringLiteral("peer-offline.json")));

        QVERIFY(status.valid);
        QCOMPARE(status.devices.size(), 1);

        const Device tv = status.devices.first();
        QVERIFY(!tv.online);
        QCOMPARE(tv.taildropTarget, TaildropTarget::Offline);
        QVERIFY(!tv.canReceiveFiles());
    }

    void readsPeerThatCannotShareFiles()
    {
        const Status status = parseStatus(fixture(QStringLiteral("no-file-sharing.json")));

        QVERIFY(status.valid);
        QCOMPARE(status.devices.size(), 1);

        const Device runner = status.devices.first();
        QVERIFY(runner.online);
        QCOMPARE(runner.taildropTarget, TaildropTarget::OwnedByOtherUser);
        QVERIFY(!runner.canReceiveFiles());
        QCOMPARE(runner.noFileSharingReason, QStringLiteral("node is owned by a different user"));
    }

    void readsBackendStates_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QString>("backendState");
        QTest::addColumn<int>("deviceCount");

        // Logged out, the daemon still answers with a Self and a Peer map: the
        // Self has an empty DNSName, and the single peer is a leftover of the
        // engine with every field empty but the byte counters of the last
        // transfer. Captured on this machine during a real "tailscale logout".
        // No device comes out of it, but for the reason in 3.4 -- a peer with
        // no DNSName is unaddressable and gets dropped -- and not because the
        // map is empty.
        QTest::newRow("needs login") << QStringLiteral("needs-login.json") << QStringLiteral("NeedsLogin") << 0;
        // A stopped backend still lists the whole tailnet, with the peers'
        // Online flags frozen at whatever they were: measured on this machine
        // by capturing "tailscale status --json" during a real "tailscale
        // down". What changes is every peer's TaildropTarget, which becomes
        // IpnStateNotRunning.
        QTest::newRow("stopped") << QStringLiteral("stopped.json") << QStringLiteral("Stopped") << 4;
    }

    void readsBackendStates()
    {
        QFETCH(QString, file);
        QFETCH(QString, backendState);
        QFETCH(int, deviceCount);

        const Status status = parseStatus(fixture(file));

        QVERIFY(status.valid);
        QCOMPARE(status.backendState, backendState);
        QVERIFY(!status.isRunning());
        QCOMPARE(status.devices.size(), deviceCount);
        // The invariant that matters, whatever the backend chose to report:
        // with it not running, nothing on the list can be sent to.
        for (const Device &device : status.devices) {
            QVERIFY(!device.canReceiveFiles());
            QCOMPARE(device.taildropTarget, TaildropTarget::IpnStateNotRunning);
        }
    }

    void handlesTailnetWithoutPeers()
    {
        const Status status = parseStatus(fixture(QStringLiteral("empty-tailnet.json")));

        QVERIFY(status.valid);
        QVERIFY(status.isRunning());
        QVERIFY(status.devices.isEmpty());
    }

    void survivesMissingAndUnknownFields()
    {
        const Status status = parseStatus(fixture(QStringLiteral("sparse.json")));

        QVERIFY(status.valid);
        // The peer without a DNS name is dropped: it cannot be addressed.
        QCOMPARE(status.devices.size(), 2);

        const Device mystery = status.devices.at(0);
        QVERIFY(mystery.hostName.isEmpty());
        QCOMPARE(mystery.displayName(), QStringLiteral("mystery"));
        QVERIFY(!mystery.online);
        QCOMPARE(mystery.taildropTarget, TaildropTarget::Unknown);

        // A value added by a future tailscale must degrade, not crash.
        const Device future = status.devices.at(1);
        QCOMPARE(future.taildropTarget, TaildropTarget::Unknown);
        QVERIFY(!future.canReceiveFiles());
    }

    void rejectsMalformedJson()
    {
        const Status status = parseStatus(fixture(QStringLiteral("malformed.json")));

        QVERIFY(!status.valid);
        QVERIFY(!status.error.isEmpty());
        QVERIFY(status.devices.isEmpty());
    }

    void rejectsNonObjectJson()
    {
        const Status status = parseStatus(QByteArrayLiteral("[1, 2, 3]"));

        QVERIFY(!status.valid);
        QVERIFY(!status.error.isEmpty());
    }

    void rejectsEmptyOutput()
    {
        const Status status = parseStatus(QByteArray());

        QVERIFY(!status.valid);
        QVERIFY(!status.error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(StatusParserTest)

#include "statusparsertest.moc"
