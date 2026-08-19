/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QElapsedTimer>
#include <QTest>

#include "tailscaleclient.h"

using namespace Tailshare;

class TailscaleClientTest : public QObject
{
    Q_OBJECT

private:
    /** A client that reads a fixture instead of talking to a real tailscaled. */
    static TailscaleClient clientReading(const QString &fixture)
    {
        TailscaleClient client;
        client.setProgram(QStringLiteral("/bin/cat"));
        client.setArguments({QStringLiteral(FIXTURE_DIR "/") + fixture});
        return client;
    }

    static TailscaleClient clientRunning(const QString &script)
    {
        TailscaleClient client;
        client.setProgram(QStringLiteral("/bin/sh"));
        client.setArguments({QStringLiteral("-c"), script});
        return client;
    }

private Q_SLOTS:
    /** No test inherits an override from whoever started ctest. */
    void init()
    {
        qunsetenv("TAILSHARE_TAILSCALE");
    }

    void cleanup()
    {
        qunsetenv("TAILSHARE_TAILSCALE");
    }

    void parsesTheOutputOfTheCommand()
    {
        const Status status = clientReading(QStringLiteral("running-tailnet.json")).fetchStatus();

        QVERIFY(status.valid);
        QVERIFY(status.isRunning());
        QCOMPARE(status.devices.size(), 4);
    }

    void takesAnAbsoluteOverride()
    {
        qputenv("TAILSHARE_TAILSCALE", "/bin/cat");

        QCOMPARE(TailscaleClient().program(), QStringLiteral("/bin/cat"));
    }

    /**
     * A bare name would be resolved against the working directory of whoever
     * loaded the plugin, which for Dolphin is a folder the user chose. Refuse
     * it rather than run something out of there.
     */
    void refusesARelativeOverride()
    {
        qputenv("TAILSHARE_TAILSCALE", "tailscale");

        TailscaleClient client;
        QVERIFY(client.program().isEmpty());

        const Status status = client.fetchStatus();
        QVERIFY(!status.valid);
        QVERIFY2(status.error.contains(QStringLiteral("absolute")), qPrintable(status.error));
    }

    void reportsMissingExecutable()
    {
        TailscaleClient client;
        client.setProgram(QString());

        const Status status = client.fetchStatus();

        QVERIFY(!status.valid);
        QVERIFY(status.error.contains(QStringLiteral("PATH")));
    }

    void reportsExecutableThatCannotRun()
    {
        TailscaleClient client;
        client.setProgram(QStringLiteral("/nonexistent/tailscale"));

        const Status status = client.fetchStatus();

        QVERIFY(!status.valid);
        QVERIFY(!status.error.isEmpty());
    }

    void givesUpOnASlowBackend()
    {
        TailscaleClient client = clientRunning(QStringLiteral("sleep 5"));
        client.setTimeout(200);

        QElapsedTimer clock;
        clock.start();
        const Status status = client.fetchStatus();
        const qint64 elapsed = clock.elapsed();

        QVERIFY(!status.valid);
        QVERIFY(status.error.contains(QStringLiteral("did not answer")));
        // The context menu is on the other side of this call: it must come back
        // in roughly the budget, not in five seconds.
        QVERIFY2(elapsed < 2000, qPrintable(QStringLiteral("took %1 ms").arg(elapsed)));
    }

    void reportsANonZeroExit()
    {
        const Status status = clientRunning(QStringLiteral("echo 'is tailscaled running?' >&2; exit 3")).fetchStatus();

        QVERIFY(!status.valid);
        QVERIFY(status.error.contains(QStringLiteral("3")));
        // The tailscale message is the useful part of the failure; keep it.
        QVERIFY(status.error.contains(QStringLiteral("is tailscaled running?")));
    }

    void reportsGarbageOutput()
    {
        const Status status = clientRunning(QStringLiteral("echo 'not json at all'")).fetchStatus();

        QVERIFY(!status.valid);
        QVERIFY(!status.error.isEmpty());
    }

    void defaultsToTheStatusJsonCommand()
    {
        const TailscaleClient client;

        QCOMPARE(client.arguments(), QStringList({QStringLiteral("status"), QStringLiteral("--json")}));
        QCOMPARE(client.timeout(), TailscaleClient::DefaultTimeoutMs);
    }
};

QTEST_GUILESS_MAIN(TailscaleClientTest)

#include "tailscaleclienttest.moc"
