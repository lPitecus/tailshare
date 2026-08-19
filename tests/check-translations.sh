#!/bin/sh
# SPDX-FileCopyrightText: 2026 Arthur Silva
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Fails when a catalog stops matching the code: a string added to src/ and never
# translated, or a translation left over from a string that is gone. Every
# language under po/ is checked, so adding one adds it here too.
# Takes the source directory as its only argument.

set -eu

srcdir=$1

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

podir=$tmpdir "$srcdir/Messages.sh"

found=0
for catalog in "$srcdir"/po/*/tailshare.po; do
    [ -e "$catalog" ] || continue
    found=$((found + 1))
    echo "checking $catalog"
    # msgcmp is the whole check: it fails both on a msgid missing from the
    # catalog and on one that is there with an empty translation.
    msgcmp "$catalog" "$tmpdir/tailshare.pot"
    # And this one catches a placeholder dropped or renumbered in a translation.
    msgfmt --check -o /dev/null "$catalog"
done

if [ "$found" -eq 0 ]; then
    echo "no catalog found under $srcdir/po" >&2
    exit 1
fi
