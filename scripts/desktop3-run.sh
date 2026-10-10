#!/bin/bash
# SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
# SPDX-License-Identifier: CC0-1.0
#
# Dev only: runs build/bin/plasma-mail with its windows forced onto Desktop 3 and never
# activated, so a GUI test run does not pull the user away from the desktop they are on.
# The KWin rule exists only while the app runs. Usage: scripts/desktop3-run.sh [ENV=VALUE…]
# (e.g. PLASMAMAIL_SCREENSHOT=/path/shot.png).
set -u
cd "$(dirname "$0")/.."
rules=$HOME/.config/kwinrulesrc
group=plasmamail-test-desktop3
uuid=$(busctl --user get-property org.kde.KWin /VirtualDesktopManager org.kde.KWin.VirtualDesktopManager desktops \
    | grep -o '"[0-9a-f-]\{36\}" "Desktop 3"' | cut -d'"' -f2)
[ -n "$uuid" ] || { echo "Desktop 3 not found" >&2; exit 1; }

backup=$(mktemp)
cp -a "$rules" "$backup" 2>/dev/null || printf '[General]\ncount=0\nrules=\n' >"$backup"
cleanup() {
    cp -a "$backup" "$rules" && rm -f "$backup"
    qdbus6 org.kde.KWin /KWin reconfigure
}
trap cleanup EXIT

kwriteconfig6 --file "$rules" --group "$group" --key Description "Plasma Mail dev run on Desktop 3"
kwriteconfig6 --file "$rules" --group "$group" --key wmclass plasma-mail
kwriteconfig6 --file "$rules" --group "$group" --key wmclassmatch 1
kwriteconfig6 --file "$rules" --group "$group" --key desktops "$uuid"
kwriteconfig6 --file "$rules" --group "$group" --key desktopsrule 4
kwriteconfig6 --file "$rules" --group "$group" --key fsplevel 4
kwriteconfig6 --file "$rules" --group "$group" --key fsplevelrule 4
kwriteconfig6 --file "$rules" --group General --key rules "$group"
kwriteconfig6 --file "$rules" --group General --key count 1
qdbus6 org.kde.KWin /KWin reconfigure
sleep 1
env "$@" build/bin/plasma-mail
