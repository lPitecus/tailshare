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

        QTest::newRow("needs login") << QStringLiteral("needs-login.json") << QStringLiteral("NeedsLogin");
        QTest::newRow("stopped") << QStringLiteral("stopped.json") << QStringLiteral("Stopped");
    }

    void readsBackendStates()
    {
        QFETCH(QString, file);
        QFETCH(QString, backendState);

        const Status status = parseStatus(fixture(file));

        QVERIFY(status.valid);
        QCOMPARE(status.backendState, backendState);
        QVERIFY(!status.isRunning());
        QVERIFY(status.devices.isEmpty());
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
