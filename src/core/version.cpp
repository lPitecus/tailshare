/*
    SPDX-FileCopyrightText: 2026 Arthur Silva
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "version.h"

QString Tailshare::version()
{
    return QStringLiteral(TAILSHARE_VERSION_STRING);
}
