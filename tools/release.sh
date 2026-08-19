#!/bin/bash
# SPDX-FileCopyrightText: 2026 Arthur Silva
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Cuts a release: tools/release.sh 0.2.0
#
# The version lives in exactly one place, project() in CMakeLists.txt. This
# carries it to the tag, to the package and to the release notes, in that order,
# and refuses to start if anything about the tree would make the result a lie.

set -euo pipefail

cd "$(dirname "$0")/.."

die() { printf '\n\033[31m%s\033[0m\n' "$*" >&2; exit 1; }
step() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }

version=${1:-}
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "usage: tools/release.sh <major.minor.patch>"
tag="v$version"

# --- refusals, before a single file is written -------------------------------

step "Checking the tree"
[[ -z "$(git status --porcelain)" ]] || die "the working tree has changes; commit or stash them first"
[[ "$(git branch --show-current)" == "main" ]] || die "releases are cut from main"
git fetch --quiet origin
[[ -z "$(git log origin/main..HEAD)" ]] || die "HEAD is ahead of origin/main; push first"
[[ -z "$(git log HEAD..origin/main)" ]] || die "origin/main is ahead of HEAD; pull first"
! git rev-parse -q --verify "refs/tags/$tag" >/dev/null || die "$tag already exists"

current=$(sed -n 's/^project(tailshare VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
[[ -n "$current" ]] || die "could not read the current version from CMakeLists.txt"
[[ "$current" != "$version" ]] || die "CMakeLists.txt already says $version"
printf '   %s -> %s\n' "$current" "$version"

# The notes come from the changelog, so it has to have something to say.
notes=$(awk '/^## \[Unreleased\]/{f=1;next} /^## \[/{f=0} f' CHANGELOG.md | sed '/^[[:space:]]*$/d')
[[ -n "$notes" ]] || die "CHANGELOG.md has nothing under [Unreleased]"

printf '\nThese are the notes this release will carry:\n\n%s\n' "$notes"
read -rp $'\nRelease '"$version"'? [y/N] ' answer
[[ "$answer" == [yY] ]] || die "nothing done"

# --- the version, in its one place -------------------------------------------

step "Bumping CMakeLists.txt and CHANGELOG.md"
sed -i "s/^project(tailshare VERSION $current/project(tailshare VERSION $version/" CMakeLists.txt
today=$(date +%Y-%m-%d)
sed -i "s|^## \[Unreleased\]|## [Unreleased]\n\n## [$version] - $today|" CHANGELOG.md
sed -i "s|^\[Unreleased\]:.*|[Unreleased]: https://github.com/lPitecus/tailshare/compare/$tag...HEAD\n[$version]: https://github.com/lPitecus/tailshare/compare/v$current...$tag|" CHANGELOG.md

step "Building and testing, before anything is published"
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null
cmake --build build -j"$(nproc)" >/dev/null
ctest --test-dir build --output-on-failure

step "Committing and tagging"
git add CMakeLists.txt CHANGELOG.md
git commit -q -m "Release $version" -m "$notes"
git tag -a "$tag" -m "tailshare $version" -m "$notes"
git push --quiet origin main
git push --quiet origin "$tag"

# --- the package, which cannot exist until the tag is public ------------------

step "Updating the package from the published tarball"
sed -i "s/^pkgver=.*/pkgver=$version/;s/^pkgrel=.*/pkgrel=1/" packaging/PKGBUILD
( cd packaging && updpkgsums )
( cd packaging && rm -rf src pkg && makepkg -f --noconfirm )
( cd packaging && rm -rf src pkg ./*.pkg.tar.zst ./*.tar.gz )

git add packaging/PKGBUILD
git commit -q -m "Package $version" -m "Built from the $tag tarball, checksum and all."
git push --quiet origin main

step "Publishing the release"
notes_file=$(mktemp)
printf '%s\n' "$notes" > "$notes_file"
gh release create "$tag" --title "tailshare $version" --notes-file "$notes_file" --verify-tag
rm -f "$notes_file"

step "Done: https://github.com/lPitecus/tailshare/releases/tag/$tag"
