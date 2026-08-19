/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

#include "sendjob.h"

namespace Tailshare
{

/**
 * The words a transfer is described with, in one place.
 *
 * SendJob reports states, not sentences; both the Plasma notification and the
 * command line probe turn those states into text through here, so there is a
 * single set of strings to translate and a single wording to keep consistent.
 */
namespace SendMessages
{

/** Short headline: "Sending files", "Files sent", "Send failed". */
QString title(const SendJob &job);

/** The detail line, naming the device and what is going to it. */
QString text(const SendJob &job);

/** Theme icon for the current state; never a branded logo. */
QString iconName(const SendJob &job);

/** The notifyrc event this state belongs to: sending, sent or error. */
QString eventId(SendJob::State state);

}
}
