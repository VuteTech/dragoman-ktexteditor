#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
# SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the release source tarball dragoman-ktexteditor-<version>.tar.gz:
# the git tree at HEAD plus a .tarball-version file, from which CMake takes
# the version (the checkout itself has none; versions live in git tags).
# The archive is reproducible: sorted entries, fixed owner, and the commit
# time as every file's mtime.
#
# The version comes from --version, or else from a vX.Y.Z tag on HEAD.
#
# Usage: scripts/make-release-tarball.sh [--version X.Y.Z] [output-dir]

set -euo pipefail

usage="usage: make-release-tarball.sh [--version X.Y.Z] [output-dir]"
version=""
if [ "${1:-}" = "--version" ]; then
    version="${2:?$usage}"
    shift 2
fi
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
out_dir="${1:-$root/build-release}"
mkdir -p "$out_dir"
out_dir="$(cd -- "$out_dir" && pwd)"

if [ -z "$version" ]; then
    tag="$(git -C "$root" describe --tags --exact-match --match 'v[0-9]*' HEAD 2>/dev/null)" || {
        echo "HEAD carries no vX.Y.Z tag; pass --version X.Y.Z" >&2
        exit 2
    }
    version="${tag#v}"
fi
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    echo "version must look like 1.2.3, got: $version" >&2
    exit 2
}

name="dragoman-ktexteditor-$version"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

git -C "$root" archive --format=tar --prefix="$name/" HEAD | tar -x -C "$tmp"
echo "$version" >"$tmp/$name/.tarball-version"

mtime="@$(git -C "$root" log -1 --format=%ct HEAD)"
tar -C "$tmp" --sort=name --mtime="$mtime" --owner=0 --group=0 --numeric-owner \
    -cf - "$name" | gzip -n -9 >"$out_dir/$name.tar.gz"
sha256sum "$out_dir/$name.tar.gz"
