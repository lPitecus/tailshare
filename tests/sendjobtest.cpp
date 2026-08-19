/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "sendjob.h"
#include "sendplan.h"

using namespace Tailshare;

/**
 * Exercises the whole send path with a shell script standing in for tailscale.
 *
 * Nothing here touches a tailnet: what matters is that the states, the command
 * line, the temporary archive and the failure reporting behave, which a fake
 * executable shows more precisely than a real transfer could.
 */
class SendJobTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    Device m_target;

    QString path(const QString &name) const
    {
        return m_dir.filePath(name);
    }

    QString capturePath() const
    {
        return path(QStringLiteral("argv.txt"));
    }

    void makeFile(const QString &name, const QByteArray &content = QByteArrayLiteral("tailshare test\n"))
    {
        QVERIFY(QDir(m_dir.path()).mkpath(QFileInfo(name).path()));
        QFile file(path(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(content);
    }

    /** Writes an executable shell script and returns its path, empty on failure. */
    QString makeScript(const QString &name, const QString &body)
    {
        const QString scriptPath = path(name);
        QFile script(scriptPath);
        if (!script.open(QIODevice::WriteOnly)) {
            return QString();
        }
        script.write(QStringLiteral("#!/bin/sh\n%1\n").arg(body).toUtf8());
        script.close();
        if (!script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
            return QString();
        }
        return scriptPath;
    }

    /** The arguments the fake tailscale was called with, one per line. */
    QStringList capturedArguments() const
    {
        QFile file(capturePath());
        if (!file.open(QIODevice::ReadOnly)) {
            return QStringList();
        }
        return QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }

    SendPlan planFor(const QStringList &names) const
    {
        QStringList paths;
        for (const QString &name : names) {
            paths.append(path(name));
        }
        return SendPlan::build(paths, m_target);
    }

    static QList<SendJob::State> statesOf(const QSignalSpy &spy)
    {
        QList<SendJob::State> states;
        for (const QList<QVariant> &call : spy) {
            states.append(call.first().value<SendJob::State>());
        }
        return states;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());

        makeFile(QStringLiteral("notes.txt"));
        makeFile(QStringLiteral("relatório final.txt"));
        makeFile(QStringLiteral("Fotos/praia.jpg"), QByteArrayLiteral("jpeg"));

        m_target.hostName = QStringLiteral("home-nas");
        m_target.dnsName = QStringLiteral("home-nas.tail1234.ts.net.");
        m_target.online = true;
        m_target.taildropTarget = TaildropTarget::Available;
    }

    void init()
    {
        QFile::remove(capturePath());
    }

    void sendsPlainFilesWithoutCompressing()
    {
        const SendPlan plan = planFor({QStringLiteral("notes.txt"), QStringLiteral("relatório final.txt")});
        SendJob job(plan, m_target);
        job.setProgram(makeScript(QStringLiteral("ok.sh"), QStringLiteral("printf '%s\\n' \"$@\" > '") + capturePath() + QStringLiteral("'")));

        QSignalSpy states(&job, &SendJob::stateChanged);
        QSignalSpy finished(&job, &SendJob::finished);
        job.start();

        QTRY_VERIFY_WITH_TIMEOUT(job.isFinished(), 5000);
        QCOMPARE(job.state(), SendJob::State::Succeeded);
        QCOMPARE(statesOf(states), QList<SendJob::State>({SendJob::State::Sending, SendJob::State::Succeeded}));
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().first().toBool(), true);
        QCOMPARE(capturedArguments(), plan.commandArguments());
    }

    void compressesWhenTheSelectionHasAFolder()
    {
        const SendPlan plan = planFor({QStringLiteral("Fotos")});
        QVERIFY(plan.needsArchive());

        SendJob job(plan, m_target);
        job.setProgram(makeScript(QStringLiteral("ok.sh"), QStringLiteral("printf '%s\\n' \"$@\" > '") + capturePath() + QStringLiteral("'")));

        QSignalSpy states(&job, &SendJob::stateChanged);
        job.start();

        QTRY_VERIFY_WITH_TIMEOUT(job.isFinished(), 5000);
        QCOMPARE(job.state(), SendJob::State::Succeeded);
        QCOMPARE(statesOf(states), QList<SendJob::State>({SendJob::State::Compressing, SendJob::State::Sending, SendJob::State::Succeeded}));

        // What went out was the archive, not the folder tailscale would refuse.
        const QStringList arguments = capturedArguments();
        QCOMPARE(arguments.size(), 5);
        QVERIFY(arguments.at(3).endsWith(QStringLiteral("/Fotos.zip")));
        QCOMPARE(arguments.last(), plan.targetArgument());
    }

    void removesTheTemporaryArchiveAfterwards()
    {
        QString archivePath;
        {
            SendJob job(planFor({QStringLiteral("Fotos")}), m_target);
            job.setProgram(makeScript(QStringLiteral("ok.sh"), QStringLiteral("printf '%s\\n' \"$@\" > '") + capturePath() + QStringLiteral("'")));
            job.start();
            QTRY_VERIFY_WITH_TIMEOUT(job.isFinished(), 5000);

            archivePath = capturedArguments().at(3);
            QVERIFY(QFile::exists(archivePath));
        }
        QVERIFY(!QFile::exists(archivePath));
    }

    void reportsTheToolsOwnErrorMessage()
    {
        SendJob job(planFor({QStringLiteral("notes.txt")}), m_target);
        job.setProgram(makeScript(QStringLiteral("fail.sh"), QStringLiteral("echo 'refused: no such peer' >&2\nexit 3")));

        job.start();
        QTRY_VERIFY_WITH_TIMEOUT(job.isFinished(), 5000);

        QCOMPARE(job.state(), SendJob::State::Failed);
        QCOMPARE(job.errorText(), QStringLiteral("refused: no such peer"));
    }

    void reportsANonZeroExitWithoutOutput()
    {
        SendJob job(planFor({QStringLiteral("notes.txt")}), m_target);
        job.setProgram(makeScript(QStringLiteral("quiet.sh"), QStringLiteral("exit 7")));

        job.start();
        QTRY_VERIFY_WITH_TIMEOUT(job.isFinished(), 5000);

        QCOMPARE(job.state(), SendJob::State::Failed);
        QVERIFY(job.errorText().contains(QStringLiteral("7")));
    }

    void failsWhenTheExecutableIsMissing()
    {
        SendJob job(planFor({QStringLiteral("notes.txt")}), m_target);
        job.setProgram(path(QStringLiteral("does-not-exist")));

        job.start();
        QTRY_VERIFY_WITH_TIMEOUT(job.isFinished(), 5000);

        QCOMPARE(job.state(), SendJob::State::Failed);
        QVERIFY(!job.errorText().isEmpty());
    }

    void failsWhenThereIsNoExecutableAtAll()
    {
        SendJob job(planFor({QStringLiteral("notes.txt")}), m_target);
        job.setProgram(QString());

        job.start();

        QCOMPARE(job.state(), SendJob::State::Failed);
        QVERIFY(job.errorText().contains(QStringLiteral("PATH")));
    }

    void refusesAnInvalidPlan()
    {
        const SendPlan plan = planFor({QStringLiteral("ghost.txt")});
        QVERIFY(!plan.isValid());

        SendJob job(plan, m_target);
        job.setProgram(makeScript(QStringLiteral("ok.sh"), QStringLiteral("exit 0")));
        QSignalSpy finished(&job, &SendJob::finished);

        job.start();

        QCOMPARE(job.state(), SendJob::State::Failed);
        QCOMPARE(job.errorText(), plan.error());
        QCOMPARE(finished.count(), 1);
        // Nothing may have been run.
        QVERIFY(capturedArguments().isEmpty());
    }

    void timeoutIsDisabledByDefault()
    {
        SendJob job(planFor({QStringLiteral("notes.txt")}), m_target);
        QCOMPARE(job.timeout(), 0);
    }

    void honoursAnExplicitTimeout()
    {
        SendJob job(planFor({QStringLiteral("notes.txt")}), m_target);
        job.setProgram(makeScript(QStringLiteral("slow.sh"), QStringLiteral("sleep 5")));
        job.setTimeout(300);

        QElapsedTimer clock;
        clock.start();
        job.start();
        QTRY_VERIFY_WITH_TIMEOUT(job.isFinished(), 3000);

        QCOMPARE(job.state(), SendJob::State::Failed);
        QVERIFY(job.errorText().contains(QStringLiteral("300 ms")));
        QVERIFY(clock.elapsed() < 2000);
    }

    void cancellingATransferEndsAsCanceled()
    {
        SendJob job(planFor({QStringLiteral("notes.txt")}), m_target);
        job.setProgram(makeScript(QStringLiteral("slow.sh"), QStringLiteral("sleep 5")));

        QSignalSpy finished(&job, &SendJob::finished);
        job.start();
        QTRY_COMPARE_WITH_TIMEOUT(job.state(), SendJob::State::Sending, 2000);
        job.cancel();

        QTRY_VERIFY_WITH_TIMEOUT(job.isFinished(), 5000);
        QCOMPARE(job.state(), SendJob::State::Canceled);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().first().toBool(), false);
    }

    void cancellingBeforeStartingRunsNothing()
    {
        SendJob job(planFor({QStringLiteral("notes.txt")}), m_target);
        job.setProgram(makeScript(QStringLiteral("ok.sh"), QStringLiteral("printf '%s\\n' \"$@\" > '") + capturePath() + QStringLiteral("'")));

        job.cancel();
        job.start();

        QCOMPARE(job.state(), SendJob::State::Canceled);
        QTest::qWait(200);
        QVERIFY(capturedArguments().isEmpty());
    }
};

QTEST_GUILESS_MAIN(SendJobTest)

#include "sendjobtest.moc"
