/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <KAbstractFileItemActionPlugin>
#include <KFileItem>
#include <KFileItemActions>
#include <KFileItemListProperties>
#include <KPluginFactory>
#include <KPluginMetaData>

#include <QAction>
#include <QDir>
#include <QLocale>
#include <QElapsedTimer>
#include <QFile>
#include <QMenu>
#include <QMimeDatabase>
#include <QTemporaryDir>
#include <QTest>
#include <QWidget>

/**
 * Drives the real plugin binary the way KIO does.
 *
 * The tailnet is scripted through $TAILSHARE_TAILSCALE, so the menu is checked
 * against known status output instead of whatever this machine's tailnet
 * happens to look like. The MIME cases mirror the matching KIO performs before
 * the plugin is ever loaded, which is the part that decides whether the submenu
 * shows up at all.
 */
class PluginTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QWidget m_window;

    QString path(const QString &name) const
    {
        return m_dir.filePath(name);
    }

    void makeFile(const QString &name, const QByteArray &content)
    {
        QFile file(path(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(content);
    }

    /** A fake tailscale that prints one of the status fixtures. */
    QString fakeTailscale(const QString &name, const QString &body)
    {
        const QString scriptPath = path(name);
        QFile script(scriptPath);
        if (!script.open(QIODevice::WriteOnly)) {
            return QString();
        }
        script.write(QStringLiteral("#!/bin/sh\n%1\n").arg(body).toUtf8());
        script.close();
        script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        return scriptPath;
    }

    QString fixture(const QString &name) const
    {
        return QStringLiteral(FIXTURE_DIR "/") + name;
    }

    static KPluginMetaData metaData()
    {
        return KPluginMetaData(QStringLiteral(PLUGIN_PATH));
    }

    KAbstractFileItemActionPlugin *loadPlugin()
    {
        auto result = KPluginFactory::instantiatePlugin<KAbstractFileItemActionPlugin>(metaData(), this);
        return result.plugin;
    }

    static KFileItemListProperties propertiesFor(const QStringList &paths)
    {
        KFileItemList items;
        for (const QString &path : paths) {
            items.append(KFileItem(QUrl::fromLocalFile(path)));
        }
        return KFileItemListProperties(items);
    }

    /**
     * The test KIO applies before loading a plugin.
     * @see kfileitemactions.cpp, where the empty common type of a file-only
     * selection falls back to application/octet-stream.
     */
    static bool kioWouldMatch(const KFileItemListProperties &properties, const QStringList &declared)
    {
        QString commonMimeType = properties.mimeType();
        if (commonMimeType.isEmpty() && properties.isFile()) {
            commonMimeType = QStringLiteral("application/octet-stream");
        }

        const QMimeType mimeType = QMimeDatabase().mimeTypeForName(commonMimeType);
        for (const QString &candidate : declared) {
            if (mimeType.inherits(candidate)) {
                return true;
            }
        }
        return false;
    }

    /** Where @p title sits in the menu tree, empty when it is not there. */
    static QString findEntry(const QMenu *menu, const QString &title, const QString &prefix)
    {
        for (const QAction *action : menu->actions()) {
            const QString here = prefix.isEmpty() ? action->text() : prefix + QStringLiteral(" > ") + action->text();
            if (action->text() == title) {
                return here;
            }
            if (action->menu()) {
                const QString found = findEntry(action->menu(), title, here);
                if (!found.isEmpty()) {
                    return found;
                }
            }
        }
        return QString();
    }

    /** The whole menu tree as one line, for a failure message worth reading. */
    static QString describe(const QMenu *menu, const QString &prefix)
    {
        QStringList entries;
        for (const QAction *action : menu->actions()) {
            if (action->isSeparator()) {
                continue;
            }
            const QString here = prefix.isEmpty() ? action->text() : prefix + QStringLiteral(" > ") + action->text();
            entries.append(action->menu() ? describe(action->menu(), here) : here);
        }
        return entries.join(QStringLiteral(", "));
    }

    /** The device names of a built submenu, in order, with their enabled state. */
    static QStringList menuEntries(const QList<QAction *> &actions)
    {
        QStringList entries;
        if (actions.size() != 1 || !actions.first()->menu()) {
            return entries;
        }
        const QList<QAction *> items = actions.first()->menu()->actions();
        entries.reserve(items.size());
        for (const QAction *item : items) {
            entries.append(QStringLiteral("%1 [%2]").arg(item->text(), item->isEnabled() ? QStringLiteral("on") : QStringLiteral("off")));
        }
        return entries;
    }

private Q_SLOTS:
    void initTestCase()
    {
        // The plugin's name and description are read out of the JSON by locale,
        // so the English expectations below need a locale with no translation.
        QLocale::setDefault(QLocale::c());

        QVERIFY(m_dir.isValid());
        makeFile(QStringLiteral("notes.txt"), QByteArrayLiteral("plain text\n"));
        makeFile(QStringLiteral("readme.txt"), QByteArrayLiteral("more text\n"));
        makeFile(QStringLiteral("photo.png"), QByteArrayLiteral("\x89PNG\r\n\x1a\n"));
        QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral("Fotos")));
        QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral("Backup")));
    }

    void cleanup()
    {
        qunsetenv("TAILSHARE_TAILSCALE");
    }

    void theBinaryCarriesItsMetadata()
    {
        const KPluginMetaData data = metaData();

        QVERIFY2(data.isValid(), PLUGIN_PATH);
        QCOMPARE(data.name(), QStringLiteral("Share via Tailscale"));
        // Both are mandatory: every regular file inherits application/octet-stream,
        // but inode/directory does not, so folders need their own entry.
        QCOMPARE(data.mimeTypes(), QStringList({QStringLiteral("application/octet-stream"), QStringLiteral("inode/directory")}));
        // Generated from project(), so this is the whole point of generating it:
        // the binary and the build cannot disagree about which version it is.
        QCOMPARE(data.version(), QStringLiteral(PROJECT_VERSION));
        // The item belongs in the main menu, not under "Actions".
        QVERIFY(!data.rawData().contains(QLatin1String("X-KDE-Show-In-Submenu")));
    }

    /**
     * The name Dolphin shows in Settings -> Context Menu comes from the JSON,
     * not from the message catalog: KPluginMetaData picks the "[pt_BR]" key
     * itself, by locale.
     */
    void carriesItsNameInEveryLanguageItHas()
    {
        QLocale::setDefault(QLocale(QStringLiteral("pt_BR")));
        const KPluginMetaData translated = metaData();

        QCOMPARE(translated.name(), QStringLiteral("Compartilhar pelo Tailscale"));
        QCOMPARE(translated.description(), QStringLiteral("Envie os arquivos selecionados para um dispositivo da sua tailnet"));

        QLocale::setDefault(QLocale::c());
        QCOMPARE(metaData().name(), QStringLiteral("Share via Tailscale"));
    }

    void kioMatchesEverySelectionShape_data()
    {
        QTest::addColumn<QStringList>("names");

        QTest::newRow("one file") << QStringList{QStringLiteral("notes.txt")};
        QTest::newRow("files of one type") << QStringList{QStringLiteral("notes.txt"), QStringLiteral("readme.txt")};
        QTest::newRow("files of mixed types") << QStringList{QStringLiteral("notes.txt"), QStringLiteral("photo.png")};
        QTest::newRow("folders only") << QStringList{QStringLiteral("Fotos"), QStringLiteral("Backup")};
        QTest::newRow("file and folder") << QStringList{QStringLiteral("notes.txt"), QStringLiteral("Fotos")};
    }

    void kioMatchesEverySelectionShape()
    {
        QFETCH(QStringList, names);

        QStringList paths;
        for (const QString &name : names) {
            paths.append(path(name));
        }

        QVERIFY(kioWouldMatch(propertiesFor(paths), metaData().mimeTypes()));
    }

    void listsEveryDeviceOnlineFirst()
    {
        qputenv("TAILSHARE_TAILSCALE", fakeTailscale(QStringLiteral("ts-ok.sh"), QStringLiteral("cat '%1'").arg(fixture(QStringLiteral("running-tailnet.json")))).toUtf8());

        KAbstractFileItemActionPlugin *plugin = loadPlugin();
        QVERIFY(plugin);

        const QList<QAction *> actions = plugin->actions(propertiesFor({path(QStringLiteral("notes.txt"))}), &m_window);

        QCOMPARE(actions.size(), 1);
        QCOMPARE(actions.first()->text(), QStringLiteral("Share via Tailscale"));
        QVERIFY(!menuEntries(actions).isEmpty());
    }

    void showsWhyADeviceCannotReceive()
    {
        qputenv("TAILSHARE_TAILSCALE", fakeTailscale(QStringLiteral("ts-nosharing.sh"), QStringLiteral("cat '%1'").arg(fixture(QStringLiteral("no-file-sharing.json")))).toUtf8());

        KAbstractFileItemActionPlugin *plugin = loadPlugin();
        QVERIFY(plugin);

        const QList<QAction *> actions = plugin->actions(propertiesFor({path(QStringLiteral("notes.txt"))}), &m_window);
        QCOMPARE(actions.size(), 1);

        const QList<QAction *> items = actions.first()->menu()->actions();
        QVERIFY(!items.isEmpty());
        for (const QAction *item : items) {
            if (!item->isEnabled()) {
                // A disabled entry that says nothing is a dead end for the user.
                QVERIFY2(!item->toolTip().isEmpty(), qPrintable(item->text()));
            }
        }
    }

    void staysAwayWhenTheBackendIsNotRunning_data()
    {
        QTest::addColumn<QString>("script");

        QTest::newRow("stopped") << QStringLiteral("cat '%1'").arg(fixture(QStringLiteral("stopped.json")));
        QTest::newRow("needs login") << QStringLiteral("cat '%1'").arg(fixture(QStringLiteral("needs-login.json")));
        QTest::newRow("empty tailnet") << QStringLiteral("cat '%1'").arg(fixture(QStringLiteral("empty-tailnet.json")));
        QTest::newRow("malformed output") << QStringLiteral("cat '%1'").arg(fixture(QStringLiteral("malformed.json")));
        QTest::newRow("command fails") << QStringLiteral("echo 'nope' >&2\nexit 1");
        QTest::newRow("hangs") << QStringLiteral("sleep 5");
    }

    void staysAwayWhenTheBackendIsNotRunning()
    {
        QFETCH(QString, script);
        qputenv("TAILSHARE_TAILSCALE", fakeTailscale(QStringLiteral("ts-case.sh"), script).toUtf8());

        KAbstractFileItemActionPlugin *plugin = loadPlugin();
        QVERIFY(plugin);

        QElapsedTimer clock;
        clock.start();
        const QList<QAction *> actions = plugin->actions(propertiesFor({path(QStringLiteral("notes.txt"))}), &m_window);

        QVERIFY(actions.isEmpty());
        // Whatever tailscale does, the context menu cannot wait for it.
        QVERIFY2(clock.elapsed() < 1000, qPrintable(QString::number(clock.elapsed())));
    }

    /**
     * The whole KIO path, not just our half of it: this is what Dolphin runs
     * when it builds a context menu, so it also shows *where* the entry lands.
     */
    void reachesTheMenuKioBuilds()
    {
        qputenv("TAILSHARE_TAILSCALE", fakeTailscale(QStringLiteral("ts-ok.sh"), QStringLiteral("cat '%1'").arg(fixture(QStringLiteral("running-tailnet.json")))).toUtf8());

        KFileItemActions kioActions;
        kioActions.setItemListProperties(propertiesFor({path(QStringLiteral("notes.txt"))}));

        QMenu menu;
        kioActions.addActionsTo(&menu);

        const QString trail = findEntry(&menu, QStringLiteral("Share via Tailscale"), QString());
        QVERIFY2(!trail.isEmpty(), qPrintable(QStringLiteral("menu was: ") + describe(&menu, QString())));
        qWarning("the entry landed at: %s", qPrintable(trail));
    }

    void staysAwayFromRemoteSelections()
    {
        qputenv("TAILSHARE_TAILSCALE", fakeTailscale(QStringLiteral("ts-ok.sh"), QStringLiteral("cat '%1'").arg(fixture(QStringLiteral("running-tailnet.json")))).toUtf8());

        KAbstractFileItemActionPlugin *plugin = loadPlugin();
        QVERIFY(plugin);

        KFileItemList items;
        items.append(KFileItem(QUrl(QStringLiteral("sftp://server/home/user/notes.txt"))));

        QVERIFY(plugin->actions(KFileItemListProperties(items), &m_window).isEmpty());
    }

    void staysAwayWhenTailscaleIsNotInstalled()
    {
        qputenv("TAILSHARE_TAILSCALE", path(QStringLiteral("no-such-binary")).toUtf8());

        KAbstractFileItemActionPlugin *plugin = loadPlugin();
        QVERIFY(plugin);

        QVERIFY(plugin->actions(propertiesFor({path(QStringLiteral("notes.txt"))}), &m_window).isEmpty());
    }

    void measuresWhatTheContextMenuPays()
    {
        // Against the real tailscale, if this machine has one: the number that
        // matters is what Dolphin's menu waits for, not what a script costs.
        if (qEnvironmentVariableIsEmpty("TAILSHARE_REAL_TAILSCALE")) {
            QSKIP("set TAILSHARE_REAL_TAILSCALE to measure against the real binary");
        }
        qputenv("TAILSHARE_TAILSCALE", qgetenv("TAILSHARE_REAL_TAILSCALE"));

        KAbstractFileItemActionPlugin *plugin = loadPlugin();
        QVERIFY(plugin);

        qint64 worst = 0;
        for (int i = 0; i < 5; ++i) {
            QElapsedTimer clock;
            clock.start();
            plugin->actions(propertiesFor({path(QStringLiteral("notes.txt"))}), &m_window);
            worst = qMax(worst, clock.elapsed());
        }
        qWarning("slowest actions() over 5 runs: %lld ms", worst);
        QVERIFY(worst < TailshareTimeoutMs);
    }

private:
    static constexpr int TailshareTimeoutMs = 300;
};

QTEST_MAIN(PluginTest)

#include "plugintest.moc"
