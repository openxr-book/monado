#!/bin/sh
# SPDX-FileCopyrightText: 2020-2024, Rylie Pavlik <rylie@ryliepavlik.com>
# SPDX-License-Identifier: CC0-1.0

# Packages produced this way are for automated use only and shouldn't be uploaded to the Debian archive.

set -e
(
    cd "$(dirname $0)"
    cd ../..
    export DEVSCRIPTS_CHECK_DIRNAME_LEVEL=0

    if [ x"$1" != x ]; then
        COMMIT_TO_PACKAGE=$1
        echo "Package version will describe commit specified on command line: ${COMMIT_TO_PACKAGE}"
        export COMMIT_TO_PACKAGE
    else
        COMMIT_TO_PACKAGE=main
        echo "Package version will describe default commit: ${COMMIT_TO_PACKAGE}"
        export COMMIT_TO_PACKAGE
    fi

    if [ x"$2" != x ]; then
        PKG_REVISION=$2
        echo "Appending custom revision suffix specified on command line: ${PKG_REVISION}"
        export PKG_REVISION
    else
        PKG_REVISION=1~bpo11~ci$(date --utc "+%Y%m%d")
        echo "Appending auto-generated revision suffix: ${PKG_REVISION}"
        export PKG_REVISION
    fi

    UPSTREAM_VER=$(git describe --exclude "v0*" "$COMMIT_TO_PACKAGE" | sed -E -e 's/^v//' -e 's/-([0-9]+)-g([0-9a-f])/+git\1.\2/')
    echo "Computed package version ${UPSTREAM_VER}"
    git archive --format=tar "--prefix=monado_${UPSTREAM_VER}/" "${COMMIT_TO_PACKAGE}" | gzip -n > "../monado_${UPSTREAM_VER}.orig.tar.gz"
    dch --newversion "${UPSTREAM_VER}-${PKG_REVISION}" --preserve "Automated CI build of commit ${COMMIT_TO_PACKAGE}"
)
