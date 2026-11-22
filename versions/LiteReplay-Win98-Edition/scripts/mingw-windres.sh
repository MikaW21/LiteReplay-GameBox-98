#!/usr/bin/env bash
set -euo pipefail
exec "$MINGW_BIN/x86_64-w64-mingw32-windres" --preprocessor="$MINGW_BIN/x86_64-w64-mingw32-gcc-posix" --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED "$@"
