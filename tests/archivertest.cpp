/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KZip>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "archiver.h"

using namespace Tailshare;

class ArchiverTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString path(const QString &name) const
    {
        return m_dir.filePath(name);
    }

    void makeFile(const QString &name, const QByteArray &content = QByteArrayLiteral("tailshare test\n"))
    {
        QVERIFY(QDir(m_dir.path()).mkpath(QFileInfo(name).path()));
        QFile file(path(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(content), content.size());
    }

    /** The link target stored for @p entry, empty when it is not a symlink. */
    static QString linkTargetOf(const QString &archivePath, const QString &entry)
    {
        KZip zip(archivePath);
        if (!zip.open(QIODevice::ReadOnly)) {
            return QString();
        }
        const KArchiveEntry *found = zip.directory()->entry(entry);
        const QString target = found && found->isFile() ? found->symLinkTarget() : QString();
        zip.close();
        return target;
    }

    /** Every entry of the archive, as "relative/path" strings. */
    static QStringList entriesOf(const QString &archivePath)
    {
        QStringList found;
        KZip zip(archivePath);
        if (!zip.open(QIODevice::ReadOnly)) {
            return found;
        }

        std::function<void(const KArchiveDirectory *, const QString &)> walk = [&](const KArchiveDirectory *dir, const QString &prefix) {
            for (const QString &name : dir->entries()) {
                const KArchiveEntry *entry = dir->entry(name);
                const QString full = prefix.isEmpty() ? name : prefix + QLatin1Char('/') + name;
                if (entry->isDirectory()) {
                    walk(static_cast<const KArchiveDirectory *>(entry), full);
                } else {
                    found.append(full);
                }
            }
        };
        walk(zip.directory(), QString());
        zip.close();

        found.sort();
        return found;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());

        makeFile(QStringLiteral("notes.txt"));
        makeFile(QStringLiteral("relatório final.txt"));
        makeFile(QStringLiteral("-dashed name.txt"));
        makeFile(QStringLiteral("Fotos/praia.jpg"), QByteArrayLiteral("jpeg"));
        makeFile(QStringLiteral("Fotos/sub/interna.jpg"), QByteArrayLiteral("jpeg"));
        makeFile(QStringLiteral("Backup/notes.txt"), QByteArrayLiteral("another one\n"));
    }

    void packsFilesAndFoldersTogether()
    {
        const QString archive = path(QStringLiteral("out1.zip"));
        const auto result = Archiver::createZip({path(QStringLiteral("Fotos")), path(QStringLiteral("notes.txt"))}, archive);

        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(!result.canceled);
        QVERIFY(QFile::exists(archive));
        QCOMPARE(entriesOf(archive), QStringList({QStringLiteral("Fotos/praia.jpg"), QStringLiteral("Fotos/sub/interna.jpg"), QStringLiteral("notes.txt")}));
    }

    /**
     * A link that points nowhere still belongs in the archive.
     *
     * KZip's addLocalDirectory() skips it and reports success anyway, so
     * without help the file would vanish from the transfer in silence.
     */
    void keepsLinksThatPointNowhere()
    {
        const QString folder = path(QStringLiteral("Links"));
        QVERIFY(QDir(m_dir.path()).mkpath(folder));
        makeFile(QStringLiteral("Links/real.txt"));
        // Relative and dangling: the one addLocalDirectory() drops.
        QVERIFY(QFile::link(QStringLiteral("nowhere.txt"), folder + QStringLiteral("/broken.txt")));
        // Relative and valid: the one it already stores, kept as a control.
        QVERIFY(QFile::link(QStringLiteral("real.txt"), folder + QStringLiteral("/valid.txt")));

        const QString archive = path(QStringLiteral("links.zip"));
        const auto result = Archiver::createZip({folder}, archive);

        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(entriesOf(archive),
                 QStringList({QStringLiteral("Links/broken.txt"), QStringLiteral("Links/real.txt"), QStringLiteral("Links/valid.txt")}));
        // Stored as written, so it still points at the same place once unpacked.
        QCOMPARE(linkTargetOf(archive, QStringLiteral("Links/broken.txt")), QStringLiteral("nowhere.txt"));
        QCOMPARE(linkTargetOf(archive, QStringLiteral("Links/valid.txt")), QStringLiteral("real.txt"));
    }

    void keepsFileContents()
    {
        const QString archive = path(QStringLiteral("out2.zip"));
        QVERIFY(Archiver::createZip({path(QStringLiteral("notes.txt"))}, archive).ok);

        KZip zip(archive);
        QVERIFY(zip.open(QIODevice::ReadOnly));
        const KArchiveEntry *entry = zip.directory()->entry(QStringLiteral("notes.txt"));
        QVERIFY(entry && entry->isFile());
        QCOMPARE(static_cast<const KArchiveFile *>(entry)->data(), QByteArrayLiteral("tailshare test\n"));
    }

    void keepsAccentedAndDashedNames()
    {
        const QString archive = path(QStringLiteral("out3.zip"));
        const auto result =
            Archiver::createZip({path(QStringLiteral("relatório final.txt")), path(QStringLiteral("-dashed name.txt")), path(QStringLiteral("Fotos"))}, archive);

        QVERIFY2(result.ok, qPrintable(result.error));
        const QStringList entries = entriesOf(archive);
        QVERIFY(entries.contains(QStringLiteral("relatório final.txt")));
        QVERIFY(entries.contains(QStringLiteral("-dashed name.txt")));
    }

    void doesNotLoseItemsThatShareAName()
    {
        // Two "notes.txt" from different folders: both have to survive.
        const QString archive = path(QStringLiteral("out4.zip"));
        const auto result = Archiver::createZip({path(QStringLiteral("notes.txt")), path(QStringLiteral("Backup/notes.txt"))}, archive);

        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(entriesOf(archive), QStringList({QStringLiteral("notes-2.txt"), QStringLiteral("notes.txt")}));
    }

    void reportsAnAlreadyRaisedCancelFlag()
    {
        const QString archive = path(QStringLiteral("out5.zip"));
        const CancelFlag flag = makeCancelFlag();
        flag->store(true);

        const auto result = Archiver::createZip({path(QStringLiteral("Fotos"))}, archive, flag);

        QVERIFY(!result.ok);
        QVERIFY(result.canceled);
        QVERIFY(result.error.isEmpty());
    }

    void refusesAnEmptySelection()
    {
        const auto result = Archiver::createZip({}, path(QStringLiteral("out6.zip")));

        QVERIFY(!result.ok);
        QVERIFY(!result.canceled);
        QVERIFY(!result.error.isEmpty());
    }

    void reportsAnUnwritableDestination()
    {
        const auto result = Archiver::createZip({path(QStringLiteral("notes.txt"))}, path(QStringLiteral("no/such/dir/out.zip")));

        QVERIFY(!result.ok);
        QVERIFY(!result.error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(ArchiverTest)

#include "archivertest.moc"
