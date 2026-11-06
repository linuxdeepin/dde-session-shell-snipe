#!/bin/sh
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

if [ "$#" -lt 1 ]; then
    echo "Usage: sh $0 /path/to/dde-lock-test [gtest arguments]" >&2
    exit 2
fi

lsan_suppressions="$(dirname "$0")/lock-password-lsan.supp"
export LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}suppressions=$lsan_suppressions"

# Do not auto-start installed desktop services on this test bus.
bus_config=$(mktemp /tmp/dss-lock-password-bus.XXXXXX)
trap 'rm -f "$bus_config"' EXIT HUP INT TERM
cat > "$bus_config" <<'XML'
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=/tmp</listen>
  <auth>EXTERNAL</auth>
  <policy context="default">
    <allow send_destination="*"/>
    <allow receive_sender="*"/>
    <allow own="*"/>
  </policy>
</busconfig>
XML

dbus-run-session --config-file="$bus_config" -- sh -c '
    export DBUS_SYSTEM_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS"
    export DSS_TEST_PRIVATE_BUS=1
    export QT_QPA_PLATFORM=offscreen
    test_binary="$1"
    shift
    exec "$test_binary" --gtest_filter="UT_LockPasswordChange.*:UT_LockPasswordGate.*:UT_LockPasswordSession.*:UT_AuthSingle.*:UT_GreeterPasswordChange.*:UT_GreeterPasswordForm.*:UT_PasswordInputWidget.*" "$@"
' sh "$@"
