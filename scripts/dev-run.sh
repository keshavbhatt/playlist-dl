#!/usr/bin/env bash
# Runs the dev build against the kf6-core24 runtime content snap, the very
# libraries the shipped snap uses. Host fonts, display and D-Bus work
# directly; Chromium's sandbox stays ON.
#
#   scripts/dev-run.sh                 # launch the app
#   scripts/dev-run.sh -- --some-flag  # pass arguments to playlist-dl
set -euo pipefail

DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${PLDL_BUILD_DIR:-$DIR/build}"

# shellcheck source=snap-runtime-env.sh
source "$DIR/scripts/snap-runtime-env.sh"

[ -d "$PLDL_RT/usr/lib/x86_64-linux-gnu" ] || {
    echo "kf6-core24 runtime snap missing: sudo snap install kf6-core24" >&2; exit 1; }

pldl_prepare_runtime_farm "$BUILD"
pldl_export_runtime_env "$BUILD"
# Route file dialogs through xdg-desktop-portal so they use the system
# dialogs (the KDE platform-theme plugin is not in the runtime snap).
export QT_QPA_PLATFORMTHEME="${QT_QPA_PLATFORMTHEME_OVERRIDE:-xdgdesktopportal}"

[ "${1:-}" = "--" ] && shift
cd "$DIR"
exec "$BUILD/src/playlist-dl" "$@"
