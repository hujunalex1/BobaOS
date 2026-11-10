#!/usr/bin/env zsh
set -e

PORT="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)}"
PORT="${PORT:-/dev/cu.usbmodem2101}"

SCRIPT_DIR="${0:A:h}"
BIN="$SCRIPT_DIR/yumo-kids-os-merged.bin"

if [ ! -f "$BIN" ]; then
  echo "❌ 找不到固件文件: $BIN"
  exit 1
fi

ESPTOOL="esptool.py"
if ! command -v esptool.py &>/dev/null; then
  if [ -f "/Users/rebuild/.espressif/python_env/idf6.1_py3.13_env/bin/python3" ]; then
    ESPTOOL="/Users/rebuild/.espressif/python_env/idf6.1_py3.13_env/bin/python3 -m esptool"
  fi
fi

echo "正在向 $PORT 烧录 CubePaw 萌宠系统 (yumo-kids-os-merged.bin)..."
eval $ESPTOOL --chip esp32s3 -p "$PORT" -b 921600 \
  --before default-reset --after hard-reset write-flash \
  --flash-mode dio --flash-size 16MB 0x0 "$BIN"

echo "✅ 烧录完成！设备已自动重启。"
