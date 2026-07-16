#!/bin/sh
set -eu

# IDEs installed through Snap export library and GUI-plugin paths belonging to
# the Snap runtime. System QEMU must not load those ABI-incompatible libraries.
qemu=${1:?usage: qemu-clean.sh QEMU [ARGS...]}
shift

exec env -i \
    HOME="${HOME:-/tmp}" \
    USER="${USER:-user}" \
    LOGNAME="${LOGNAME:-${USER:-user}}" \
    PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
    DISPLAY="${DISPLAY:-}" \
    WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-}" \
    XAUTHORITY="${XAUTHORITY:-}" \
    XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-}" \
    DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-}" \
    "$qemu" "$@"
