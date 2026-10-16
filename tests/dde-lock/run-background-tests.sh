#!/bin/sh
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: sh $0 /path/to/dde-lock-test" >&2
    exit 2
fi

exec dbus-run-session -- sh -c '
    export DBUS_SYSTEM_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS"
    export DSS_TEST_PRIVATE_BUS=1
    export QT_QPA_PLATFORM=offscreen
    exec "$1" --gtest_filter="UT_BackgroundFallback.*:UT_SystemInfo.*"
' sh "$1"
