/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <KLocalizedString>

#include "sendjob.h"
#include "sendmessages.h"
#include "taildropreason.h"

using namespace Tailshare;

// Needed so the enum can be carried in QTest data columns.
Q_DECLARE_METATYPE(Tailshare::TaildropTarget)

/**
 * The pt-BR catalog, checked where it matters: the strings the user reads.
 *
 * KLocalizedString is pointed at the catalog this build just compiled, so the
 * test passes before anything is installed, and fails as soon as the .po stops
 * covering the code — an untranslated string comes back in English, which is
 * exactly what these comparisons catch.
 */
class I18nTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    Device m_target;

    QString path(const QString &name) const
    {
        return m_dir.filePath(name);
    }

    /** A fake tailscale that accepts anything and sends nothing. */
    QString acceptingScript()
    {
        const QString scriptPath = path(QStringLiteral("ok.sh"));
        QFile script(scriptPath);
        if (!script.open(QIODevice::WriteOnly)) {
            return QString();
        }
        script.write(QByteArrayLiteral("#!/bin/sh\nexit 0\n"));
        script.close();
        script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        return scriptPath;
    }

    SendPlan planFor(const QStringList &names) const
    {
        QStringList paths;
        for (const QString &name : names) {
            paths.append(path(name));
        }
        return SendPlan::build(paths, m_target);
    }

private Q_SLOTS:
    void initTestCase()
    {
        // The catalogs live in the build tree, not in XDG_DATA_DIRS yet.
        KLocalizedString::addDomainLocaleDir(QByteArrayLiteral("tailshare"), QStringLiteral(BUILD_LOCALE_DIR));
        KLocalizedString::setLanguages({QStringLiteral("pt_BR")});

        QVERIFY(m_dir.isValid());
        for (const QString &name : {QStringLiteral("notes.txt"), QStringLiteral("readme.txt")}) {
            QFile file(path(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QByteArrayLiteral("tailshare test\n"));
        }

        m_target.hostName = QStringLiteral("home-nas");
        m_target.dnsName = QStringLiteral("home-nas.tail1234.ts.net.");
        m_target.online = true;
        m_target.taildropTarget = TaildropTarget::Available;
    }

    void shipsTheBrazilianCatalog()
    {
        QVERIFY(KLocalizedString::availableDomainTranslations(QByteArrayLiteral("tailshare")).contains(QStringLiteral("pt_BR")));
    }

    void translatesTheRefusalReasons_data()
    {
        QTest::addColumn<TaildropTarget>("target");
        QTest::addColumn<QString>("text");

        QTest::newRow("offline") << TaildropTarget::Offline << QStringLiteral("Este dispositivo está offline.");
        QTest::newRow("no file sharing") << TaildropTarget::MissingCap << QStringLiteral("O Taildrop não está habilitado neste dispositivo.");
        QTest::newRow("not running") << TaildropTarget::IpnStateNotRunning << QStringLiteral("O Tailscale não está em execução.");
        QTest::newRow("other user") << TaildropTarget::OwnedByOtherUser
                                    << QStringLiteral("Este dispositivo pertence a outro usuário. O Taildrop só funciona entre dispositivos seus.");
    }

    void translatesTheRefusalReasons()
    {
        QFETCH(TaildropTarget, target);
        QFETCH(QString, text);

        QCOMPARE(taildropReasonText(target), text);
    }

    /**
     * A real job, because the plural form is chosen at the call site: one file
     * and two files take different branches of the same catalog entry.
     */
    void translatesTheNotificationsOfARealJob_data()
    {
        QTest::addColumn<QStringList>("names");
        QTest::addColumn<QString>("sending");
        QTest::addColumn<QString>("sent");

        QTest::newRow("one file") << QStringList{QStringLiteral("notes.txt")} << QStringLiteral("Enviando 1 arquivo para home-nas")
                                  << QStringLiteral("1 arquivo enviado para home-nas");
        QTest::newRow("two files") << QStringList{QStringLiteral("notes.txt"), QStringLiteral("readme.txt")}
                                   << QStringLiteral("Enviando 2 arquivos para home-nas") << QStringLiteral("2 arquivos enviados para home-nas");
    }

    void translatesTheNotificationsOfARealJob()
    {
        QFETCH(QStringList, names);
        QFETCH(QString, sending);
        QFETCH(QString, sent);

        SendJob job(planFor(names), m_target);
        job.setProgram(acceptingScript());

        QSignalSpy states(&job, &SendJob::stateChanged);
        job.start();

        QTRY_VERIFY_WITH_TIMEOUT(states.count() >= 1, 5000);
        QCOMPARE(SendMessages::title(job), QStringLiteral("Enviando arquivos"));
        QCOMPARE(SendMessages::text(job), sending);

        QTRY_VERIFY_WITH_TIMEOUT(job.isFinished(), 5000);
        QCOMPARE(job.state(), SendJob::State::Succeeded);
        QCOMPARE(SendMessages::title(job), QStringLiteral("Arquivos enviados"));
        QCOMPARE(SendMessages::text(job), sent);
    }

    /** English is the source language: with no catalog, nothing is missing. */
    void leavesTheOriginalAloneInEnglish()
    {
        KLocalizedString::setLanguages({QStringLiteral("en_US")});

        QCOMPARE(taildropReasonText(TaildropTarget::Offline), QStringLiteral("This device is offline."));

        KLocalizedString::setLanguages({QStringLiteral("pt_BR")});
    }
};

QTEST_GUILESS_MAIN(I18nTest)

#include "i18ntest.moc"
