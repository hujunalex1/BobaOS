#!/usr/bin/env zsh
set -e

if [ -n "$1" ]; then
  PORT="$1"
else
  PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)
  PORT="${PORT:-/dev/cu.usbmodem2101}"
fi
BUILD_DIR="/Users/rebuild/Documents/nico/ai_project/CubePaw/yumo-esp32-os/artifacts/firmware"
ESPTOOL_PY="/Users/rebuild/.espressif/python_env/idf6.1_py3.13_env/bin/python3"

echo "正在向 $PORT 烧录 Yumo OS (雨墨 OS)..."
cd "$BUILD_DIR"
"$ESPTOOL_PY" -m esptool --chip esp32s3 -p "$PORT" -b 921600 \
  --before default-reset --after hard-reset write-flash \
  --flash-mode dio --flash-size 16MB --flash-freq 80m \
  0x0 bootloader/bootloader.bin \
  0x8000 partition_table/partition-table.bin \
  0xd000 ota_data_initial.bin \
  0x20000 01_factory.bin

echo "✅ Yumo OS 烧录完成！设备已自动重启。"
