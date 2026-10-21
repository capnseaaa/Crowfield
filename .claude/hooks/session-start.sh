#!/bin/sh
# Cloud sessions: install the GBA toolchain and libmgba so build.sh and test/run.sh work.
[ "$CLAUDE_CODE_REMOTE" = "true" ] || exit 0
if ! command -v arm-none-eabi-gcc >/dev/null || [ ! -d /usr/include/mgba ]; then
    apt-get install -y gcc-arm-none-eabi libmgba-dev >/dev/null 2>&1 ||
        { apt-get update >/dev/null 2>&1 && apt-get install -y gcc-arm-none-eabi libmgba-dev >/dev/null 2>&1; }
fi
python3 -c "import PIL" 2>/dev/null || pip install pillow >/dev/null 2>&1
exit 0
