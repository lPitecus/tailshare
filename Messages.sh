#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Silva
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Extracts every translatable string into po/tailshare.pot.
#
# KDE's translation infrastructure calls this with $XGETTEXT and $podir already
# set; run standalone (./Messages.sh) it falls back to plain xgettext and the
# po/ directory next to this file.

set -eu

cd "$(dirname "$0")"

podir=${podir:-$PWD/po}
mkdir -p "$podir"

# The KDE keyword set: every i18n flavour the code uses, plus the ki18n/xi18n
# variants, so a string added later is caught without touching this file.
${XGETTEXT:-xgettext \
    --from-code=UTF-8 -C --kde \
    -ci18n \
    -ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 \
    -kki18n:1 -kki18nc:1c,2 -kki18np:1,2 -kki18ncp:1c,2,3 \
    -kxi18n:1 -kxi18nc:1c,2 -kxi18np:1,2 -kxi18ncp:1c,2,3 \
    -kkxi18n:1 -kkxi18nc:1c,2 -kkxi18np:1,2 -kkxi18ncp:1c,2,3 \
    -kI18N_NOOP:1 -kI18NC_NOOP:1c,2 \
    --package-name=tailshare \
    --msgid-bugs-address=https://github.com/lPitecus/tailshare/issues} \
    $(find src -name '*.cpp' -o -name '*.h' | sort) \
    -o "$podir/tailshare.pot"
