/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "sendplan.h"

using namespace Tailshare;

class SendPlanTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    Device m_target;

    QString path(const QString &name) const
    {
        return m_dir.filePath(name);
    }

    void makeFile(const QString &name)
    {
        QFile file(path(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("tailshare test\n");
    }

    void makeDir(const QString &name)
    {
        QVERIFY(QDir(m_dir.path()).mkpath(name));
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());

        makeFile(QStringLiteral("notes.txt"));
        makeFile(QStringLiteral("relatório final.txt"));
        makeFile(QStringLiteral("-dashed name.txt"));
        makeDir(QStringLiteral("Fotos"));
        makeDir(QStringLiteral("Backup 2026"));
        QFile inner(path(QStringLiteral("Fotos/praia.jpg")));
        QVERIFY(inner.open(QIODevice::WriteOnly));
        inner.write("jpeg");

        m_target.hostName = QStringLiteral("home-nas");
        m_target.dnsName = QStringLiteral("home-nas.tail1234.ts.net.");
        m_target.online = true;
        m_target.taildropTarget = TaildropTarget::Available;
    }

    void filesOnlyAreSentRaw()
    {
        const QStringList selection{path(QStringLiteral("notes.txt")), path(QStringLiteral("relatório final.txt"))};
        const SendPlan plan = SendPlan::build(selection, m_target);

        QVERIFY(plan.isValid());
        QVERIFY(!plan.needsArchive());
        QVERIFY(plan.archiveFileName().isEmpty());
        QCOMPARE(plan.filesToSend(), selection);
    }

    void oneFolderKeepsItsOwnName()
    {
        const SendPlan plan = SendPlan::build({path(QStringLiteral("Fotos"))}, m_target);

        QVERIFY(plan.isValid());
        QVERIFY(plan.needsArchive());
        QCOMPARE(plan.archiveFileName(), QStringLiteral("Fotos.zip"));
    }

    void severalFoldersAreNamedAfterTheParent()
    {
        const SendPlan plan = SendPlan::build({path(QStringLiteral("Fotos")), path(QStringLiteral("Backup 2026"))}, m_target);

        QVERIFY(plan.isValid());
        QVERIFY(plan.needsArchive());
        QCOMPARE(plan.archiveFileName(), QDir(m_dir.path()).dirName() + QStringLiteral(".zip"));
    }

    void oneFolderAmongFilesForcesASingleArchive()
    {
        const QStringList selection{
            path(QStringLiteral("notes.txt")),
            path(QStringLiteral("Fotos")),
            path(QStringLiteral("relatório final.txt")),
        };
        const SendPlan plan = SendPlan::build(selection, m_target);

        QVERIFY(plan.isValid());
        QVERIFY(plan.needsArchive());
        // Everything goes in, including the loose files.
        QCOMPARE(plan.sourcePaths(), selection);
        QCOMPARE(plan.archiveFileName(), QDir(m_dir.path()).dirName() + QStringLiteral(".zip"));
    }

    void archiveReplacesTheWholeSelectionOnTheCommandLine()
    {
        const SendPlan plan = SendPlan::build({path(QStringLiteral("Fotos"))}, m_target);
        const QString archive = QStringLiteral("/run/user/1000/tailshare-ab12/Fotos.zip");

        QCOMPARE(plan.filesToSend(archive), QStringList{archive});
        QCOMPARE(plan.commandArguments(archive),
                 QStringList({QStringLiteral("file"),
                              QStringLiteral("cp"),
                              QStringLiteral("--"),
                              archive,
                              QStringLiteral("home-nas.tail1234.ts.net:")}));
    }

    void commandKeepsAwkwardNamesVerbatim()
    {
        const QStringList selection{path(QStringLiteral("relatório final.txt")), path(QStringLiteral("-dashed name.txt"))};
        const SendPlan plan = SendPlan::build(selection, m_target);
        const QStringList arguments = plan.commandArguments();

        // One argument per file: no quoting, no escaping, no shell in between.
        QCOMPARE(arguments.size(), 3 + selection.size() + 1);
        QCOMPARE(arguments.at(2), QStringLiteral("--"));
        QCOMPARE(arguments.at(3), selection.at(0));
        QCOMPARE(arguments.at(4), selection.at(1));
        QCOMPARE(arguments.last(), QStringLiteral("home-nas.tail1234.ts.net:"));
    }

    void targetDropsTheTrailingDotAndKeepsTheColon()
    {
        const SendPlan plan = SendPlan::build({path(QStringLiteral("notes.txt"))}, m_target);

        QCOMPARE(plan.targetArgument(), QStringLiteral("home-nas.tail1234.ts.net:"));
    }

    void relativePathsBecomeAbsolute()
    {
        const QString previous = QDir::currentPath();
        QVERIFY(QDir::setCurrent(m_dir.path()));

        const SendPlan plan = SendPlan::build({QStringLiteral("notes.txt")}, m_target);

        QVERIFY(QDir::setCurrent(previous));
        QVERIFY(plan.isValid());
        QCOMPARE(plan.sourcePaths(), QStringList{path(QStringLiteral("notes.txt"))});
    }

    void fallsBackToATimestampWhenTheParentHasNoName()
    {
        // A selection sitting directly at the file system root has no parent
        // name to borrow, so the archive is named after the moment it was made.
        const QDateTime now(QDate(2026, 8, 18), QTime(22, 45, 0));
        const SendPlan plan = SendPlan::build({QStringLiteral("/etc"), QStringLiteral("/usr")}, m_target, now);

        QVERIFY(plan.isValid());
        QVERIFY(plan.needsArchive());
        QCOMPARE(plan.archiveFileName(), QStringLiteral("tailshare-20260818-224500.zip"));
    }

    void rejectsEmptySelection()
    {
        const SendPlan plan = SendPlan::build({}, m_target);

        QVERIFY(!plan.isValid());
        QVERIFY(!plan.error().isEmpty());
        QVERIFY(plan.commandArguments().isEmpty());
    }

    void rejectsPathThatIsGone()
    {
        const SendPlan plan = SendPlan::build({path(QStringLiteral("notes.txt")), path(QStringLiteral("vanished.txt"))}, m_target);

        QVERIFY(!plan.isValid());
        QVERIFY(plan.error().contains(QStringLiteral("vanished.txt")));
        QVERIFY(plan.commandArguments().isEmpty());
    }

    void rejectsDeviceWithoutDnsName()
    {
        Device broken;
        broken.hostName = QStringLiteral("ghost");
        broken.taildropTarget = TaildropTarget::Available;

        const SendPlan plan = SendPlan::build({path(QStringLiteral("notes.txt"))}, broken);

        QVERIFY(!plan.isValid());
        QVERIFY(!plan.error().isEmpty());
    }

    void refusesToBuildACommandBeforeTheArchiveExists()
    {
        const SendPlan plan = SendPlan::build({path(QStringLiteral("Fotos"))}, m_target);

        QVERIFY(plan.isValid());
        // No archive path yet: there is nothing legitimate to run.
        QVERIFY(plan.commandArguments().isEmpty());
    }
};

QTEST_GUILESS_MAIN(SendPlanTest)

#include "sendplantest.moc"
