#!/bin/sh
# Copyright 2020, Ryan Pavlik <ryan@ryanpavlik.com>
# SPDX-License-Identifier: CC0-1.0
# Registers dpkg-mergechangelogs as a git merge driver so the gitattributes in here works right.
# defaults to local-only: calling with --global is recommended
set -e
git config "$@" merge.dpkg-mergechangelogs.name="debian/changelog merge driver"
git config "$@" merge.dpkg-mergechangelogs.driver="dpkg-mergechangelogs -m %O %A %B %A"
