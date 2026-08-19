/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

/**
 * tailshare-probe — drives a real transfer from the command line.
 *
 * A development tool, not part of the package: it exists so the whole send path
 * (plan, ZIP, tailscale, notifications, cancellation) can be exercised against
 * a real tailnet before there is any menu to click, and so a failure can be
 * blamed on the transfer rather than on the plugin. It is also the seed of the
 * detached helper planned for v2.
 *
 * Everything is printed with QTextStream on purpose: qInfo() and qDebug() are
 * swallowed in this environment (PLAN.md, section 3.4).
 */

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QTextStream>
#include <QTimer>

#include <atomic>
#include <csignal>
#include <memory>

#include "devices.h"
#include "sendjob.h"
#include "sendmessages.h"
#include "sendnotifier.h"
#include "sendplan.h"
#include "taildropreason.h"
#include "tailscaleclient.h"
#include "version.h"

using namespace Tailshare;

namespace
{

/** Raised from the signal handler; polled by the event loop. */
std::atomic_bool g_interrupted{false};

void onInterrupt(int)
{
    g_interrupted.store(true);
}

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err()
{
    static QTextStream stream(stderr);
    return stream;
}

QString stateName(SendJob::State state)
{
    switch (state) {
    case SendJob::State::Idle:
        return QStringLiteral("idle");
    case SendJob::State::Compressing:
        return QStringLiteral("compressing");
    case SendJob::State::Sending:
        return QStringLiteral("sending");
    case SendJob::State::Succeeded:
        return QStringLiteral("succeeded");
    case SendJob::State::Failed:
        return QStringLiteral("failed");
    case SendJob::State::Canceled:
        return QStringLiteral("canceled");
    }
    return QStringLiteral("?");
}

void printDevices(const DeviceList &devices)
{
    if (devices.isEmpty()) {
        out() << "no peers in this tailnet" << Qt::endl;
        return;
    }
    for (const Device &device : Devices::sorted(devices)) {
        const QString mark = device.canReceiveFiles() ? QStringLiteral("[ready]   ") : QStringLiteral("[disabled]");
        out() << mark << QStringLiteral("  %1").arg(device.displayName(), -20) << QStringLiteral("%1").arg(device.os, -10) << taildropReasonText(device)
              << Qt::endl;
    }
}

/**
 * Finds the device the user asked for.
 *
 * Matches the display name, the MagicDNS name or its first label, all case
 * insensitively, so "pixel-8", "pixel-8.tailnet.ts.net" and "PIXEL-8" all work.
 */
const Device *findDevice(const DeviceList &devices, const QString &wanted)
{
    for (const Device &device : devices) {
        const QString firstLabel = device.dnsName.section(QLatin1Char('.'), 0, 0);
        if (device.displayName().compare(wanted, Qt::CaseInsensitive) == 0 || firstLabel.compare(wanted, Qt::CaseInsensitive) == 0
            || device.dnsName.compare(wanted, Qt::CaseInsensitive) == 0) {
            return &device;
        }
    }
    return nullptr;
}

}

int main(int argc, char **argv)
{
    // The application class has to be chosen before anything else: notifications
    // need Qt6::Gui, while the default text mode must keep working over SSH.
    bool wantsNotifications = false;
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--notify") == 0) {
            wantsNotifications = true;
        }
    }

    std::unique_ptr<QCoreApplication> app;
    if (wantsNotifications) {
        app = std::make_unique<QGuiApplication>(argc, argv);
    } else {
        app = std::make_unique<QCoreApplication>(argc, argv);
    }

    QCoreApplication::setApplicationName(QStringLiteral("tailshare"));
    QCoreApplication::setApplicationVersion(version());

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Development probe for the tailshare send path. Not installed."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption listOption({QStringLiteral("l"), QStringLiteral("list")}, QStringLiteral("List the tailnet devices and exit."));
    const QCommandLineOption deviceOption({QStringLiteral("d"), QStringLiteral("device")},
                                          QStringLiteral("Device to send to: display name, MagicDNS name or its first label."),
                                          QStringLiteral("name"));
    const QCommandLineOption dryRunOption(QStringLiteral("dry-run"), QStringLiteral("Print the plan and the tailscale argv, send nothing."));
    const QCommandLineOption notifyOption(QStringLiteral("notify"), QStringLiteral("Also raise Plasma notifications (needs a running session)."));
    const QCommandLineOption timeoutOption(QStringLiteral("timeout"), QStringLiteral("Abort the transfer after <ms> milliseconds."), QStringLiteral("ms"));
    // Swapping the executable is how the slow paths (progress notification,
    // cancellation, timeout) get exercised without a huge real transfer.
    const QCommandLineOption programOption(QStringLiteral("program"), QStringLiteral("Run <path> instead of tailscale for the transfer."), QStringLiteral("path"));
    parser.addOptions({listOption, deviceOption, dryRunOption, notifyOption, timeoutOption, programOption});
    parser.addPositionalArgument(QStringLiteral("paths"), QStringLiteral("Files and folders to send."), QStringLiteral("[paths...]"));
    parser.process(*app);

    TailscaleClient client;
    // Nothing here blocks a context menu, so the 300 ms budget of the plugin
    // would only produce flaky runs on a busy machine.
    client.setTimeout(3000);

    if (client.program().isEmpty()) {
        err() << "tailscale was not found in PATH" << Qt::endl;
        return 1;
    }

    const Status status = client.fetchStatus();
    if (!status.valid) {
        err() << "could not read the tailnet status: " << status.error << Qt::endl;
        return 1;
    }
    if (!status.isRunning()) {
        err() << "tailscale is not running (backend state: " << status.backendState << ")" << Qt::endl;
        return 1;
    }

    if (parser.isSet(listOption)) {
        printDevices(status.devices);
        return 0;
    }

    const QStringList paths = parser.positionalArguments();
    if (paths.isEmpty()) {
        err() << "nothing to send: give at least one file or folder (see --help)" << Qt::endl;
        return 1;
    }

    const DeviceList eligible = Devices::eligible(status.devices);
    const Device *target = nullptr;
    if (parser.isSet(deviceOption)) {
        target = findDevice(status.devices, parser.value(deviceOption));
        if (!target) {
            err() << "no such device: " << parser.value(deviceOption) << Qt::endl;
            printDevices(status.devices);
            return 1;
        }
        if (!target->canReceiveFiles()) {
            err() << "device cannot receive files: " << taildropReasonText(*target) << Qt::endl;
            return 1;
        }
    } else if (eligible.size() == 1) {
        target = &eligible.first();
    } else {
        err() << "pick a device with --device; these can receive files:" << Qt::endl;
        printDevices(status.devices);
        return 1;
    }

    const SendPlan plan = SendPlan::build(paths, *target);
    if (!plan.isValid()) {
        err() << plan.error() << Qt::endl;
        return 1;
    }

    out() << "target:  " << target->displayName() << " (" << plan.targetArgument() << ")" << Qt::endl;
    out() << "items:   " << plan.sourcePaths().size() << Qt::endl;
    out() << "archive: " << (plan.needsArchive() ? plan.archiveFileName() : QStringLiteral("not needed")) << Qt::endl;

    if (parser.isSet(dryRunOption)) {
        const QString archivePath = plan.needsArchive() ? QStringLiteral("<tmp>/") + plan.archiveFileName() : QString();
        out() << "argv:    " << client.program() << ' ' << plan.commandArguments(archivePath).join(QLatin1Char(' ')) << Qt::endl;
        return 0;
    }

    SendJob job(plan, *target);
    if (parser.isSet(programOption)) {
        job.setProgram(parser.value(programOption));
    }
    if (parser.isSet(timeoutOption)) {
        job.setTimeout(parser.value(timeoutOption).toInt());
    }

    std::unique_ptr<SendNotifier> notifier;
    if (wantsNotifications) {
        notifier = std::make_unique<SendNotifier>(&job);
    }

    QElapsedTimer clock;
    int exitCode = 0;

    QObject::connect(&job, &SendJob::stateChanged, [&job, &clock](SendJob::State state) {
        out() << QStringLiteral("[%1 ms] %2: %3").arg(QString::number(clock.elapsed()), stateName(state), SendMessages::text(job)) << Qt::endl;
    });
    QObject::connect(&job, &SendJob::finished, [&job, &exitCode](bool ok) {
        exitCode = ok ? 0 : (job.state() == SendJob::State::Canceled ? 2 : 1);
        QCoreApplication::quit();
    });

    // Ctrl+C asks the job to stop instead of killing the probe, which is the
    // only way to see the cancellation path end to end.
    std::signal(SIGINT, onInterrupt);
    QTimer interruptPoll;
    interruptPoll.setInterval(100);
    QObject::connect(&interruptPoll, &QTimer::timeout, [&job] {
        if (g_interrupted.exchange(false)) {
            out() << "interrupted, canceling..." << Qt::endl;
            job.cancel();
        }
    });
    interruptPoll.start();

    clock.start();
    QTimer::singleShot(0, &job, &SendJob::start);
    app->exec();

    // Give the final notification a moment to reach the daemon before the
    // process (and its DBus connection) goes away.
    if (wantsNotifications) {
        QElapsedTimer grace;
        grace.start();
        while (grace.elapsed() < 500) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        }
    }

    return exitCode;
}
