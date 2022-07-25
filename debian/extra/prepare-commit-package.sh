#!/bin/sh
# SPDX-FileCopyrightText: 2020-2022, Ryan Pavlik <ryan@ryanpavlik.com>
# SPDX-License-Identifier: CC0-1.0

# Packages produced this way are for automated use only and shouldn't be uploaded to the Debian archive.

set -e
(
    cd "$(dirname $0)"
    cd ../..
    export DEVSCRIPTS_CHECK_DIRNAME_LEVEL=0

    if [ x"$1" != x ]; then
        COMMIT_TO_PACKAGE=$1
        export COMMIT_TO_PACKAGE
    else
        COMMIT_TO_PACKAGE=master
        export COMMIT_TO_PACKAGE
    fi

    if [ x"$2" != x ]; then
        PKG_REVISION=$2
        export PKG_REVISION
    else
        PKG_REVISION=1~ubuntu2204~ci$(date --utc "+%Y%m%d")
        export PKG_REVISION
    fi

    UPSTREAM_VER=$(git describe $COMMIT_TO_PACKAGE | sed -E -e 's/^v//' -e 's/-([0-9]+)-g([0-9a-f])/+git\1.\2/')
    git archive -o "../monado_${UPSTREAM_VER}.orig.tar.gz" ${COMMIT_TO_PACKAGE}
    dch --newversion "${UPSTREAM_VER}-${PKG_REVISION}" --preserve "Automated CI build of commit ${COMMIT_TO_PACKAGE}"
)
