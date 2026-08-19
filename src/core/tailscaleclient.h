/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QStringList>

#include "statusparser.h"

namespace Tailshare
{

/**
 * Runs @c "tailscale status --json" and hands back the parsed result.
 *
 * Blocking by design and bounded by a hard timeout: KIO calls the plugin's
 * @c actions() synchronously in the file manager's own thread, so a hung
 * tailscaled must never freeze the context menu. Holds no business rule; the
 * filtering lives in Devices and SendPlan.
 */
class TailscaleClient
{
public:
    /** Default budget for the whole call, in milliseconds. */
    static constexpr int DefaultTimeoutMs = 300;

    TailscaleClient();

    /**
     * Absolute path of the tailscale executable, empty when it is not installed.
     * @c $TAILSHARE_TAILSCALE overrides the PATH lookup when it is set.
     */
    static QString findExecutable();

    QString program() const;
    /** Overrides the executable; mainly for tests and unusual installations. */
    void setProgram(const QString &program);

    QStringList arguments() const;
    void setArguments(const QStringList &arguments);

    int timeout() const;
    void setTimeout(int milliseconds);

    /** Executes the command and parses its output; never throws, never blocks past the timeout. */
    Status fetchStatus() const;

private:
    QString m_program;
    QStringList m_arguments{QStringLiteral("status"), QStringLiteral("--json")};
    int m_timeout = DefaultTimeoutMs;
};

}
