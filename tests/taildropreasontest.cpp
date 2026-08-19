/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>

#include "taildropreason.h"

using namespace Tailshare;

// Needed so the enum can be carried in QTest data columns.
Q_DECLARE_METATYPE(Tailshare::TaildropTarget)

class TaildropReasonTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void availableDeviceHasNothingToExplain()
    {
        QVERIFY(taildropReasonText(TaildropTarget::Available).isEmpty());
    }

    void everyRefusalHasAText_data()
    {
        QTest::addColumn<TaildropTarget>("target");

        QTest::newRow("unknown") << TaildropTarget::Unknown;
        QTest::newRow("no netmap") << TaildropTarget::NoNetmapAvailable;
        QTest::newRow("not running") << TaildropTarget::IpnStateNotRunning;
        QTest::newRow("missing cap") << TaildropTarget::MissingCap;
        QTest::newRow("offline") << TaildropTarget::Offline;
        QTest::newRow("no peer info") << TaildropTarget::NoPeerInfo;
        QTest::newRow("unsupported os") << TaildropTarget::UnsupportedOS;
        QTest::newRow("no peer api") << TaildropTarget::NoPeerAPI;
        QTest::newRow("other user") << TaildropTarget::OwnedByOtherUser;
    }

    void everyRefusalHasAText()
    {
        QFETCH(TaildropTarget, target);

        QVERIFY(!taildropReasonText(target).isEmpty());
    }

    void neverRepeatsTheBackendWording()
    {
        Device device;
        device.taildropTarget = TaildropTarget::OwnedByOtherUser;
        device.noFileSharingReason = QStringLiteral("node is owned by a different user");

        const QString shown = taildropReasonText(device);

        QVERIFY(!shown.isEmpty());
        QVERIFY(shown != device.noFileSharingReason);
    }

    void distinguishesTheCommonCases()
    {
        // Offline and "no Taildrop here" are the two the user actually meets,
        // and they must not collapse into the same sentence.
        QVERIFY(taildropReasonText(TaildropTarget::Offline) != taildropReasonText(TaildropTarget::MissingCap));
    }

    void unknownValueFallsBackToTheGenericText()
    {
        QCOMPARE(taildropReasonText(taildropTargetFromValue(4242)), taildropReasonText(TaildropTarget::Unknown));
    }
};

QTEST_GUILESS_MAIN(TaildropReasonTest)

#include "taildropreasontest.moc"
