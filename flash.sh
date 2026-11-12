#!/usr/bin/env zsh
# BobaOS flasher (macOS).
#
#   ./flash.sh              整机固件更新，但保留 nvs（Wi-Fi 配网、DeepSeek 凭据、
#                           闹钟等设置都不丢）。按偏移分别写 bootloader/分区表/
#                           ota_data/app，不碰 0x9000-0xD000 的 nvs 分区。
#   ./flash.sh --app        只更新 app 分区（0x20000），最快，约 6MB。
#   ./flash.sh --erase      恢复出厂：整片擦除后重新写入，配网和凭据会被清空。
#   ./flash.sh --merged     直接刷合并包 boba-os-merged.bin（0x0 起），
#                           等价于 --erase 的效果，仅在没有 artifacts 时使用。
#
# 所有模式都可以再跟一个串口，例如：./flash.sh --app /dev/cu.usbmodem101
set -e

MODE="full"
case "$1" in
  --app)    MODE="app";    shift ;;
  --erase)  MODE="erase";  shift ;;
  --merged) MODE="merged"; shift ;;
  --full)   MODE="full";   shift ;;
  --*)      echo "❌ 未知参数: $1"; exit 1 ;;
esac

PORT="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)}"
PORT="${PORT:-/dev/cu.usbmodem2101}"

SCRIPT_DIR="${0:A:h}"
ART="$SCRIPT_DIR/yumo-esp32-os/artifacts/firmware"
MERGED="$SCRIPT_DIR/boba-os-merged.bin"

ESPTOOL="esptool.py"
if ! command -v esptool.py &>/dev/null; then
  if [ -f "/Users/rebuild/.espressif/python_env/idf6.1_py3.13_env/bin/python3" ]; then
    ESPTOOL="/Users/rebuild/.espressif/python_env/idf6.1_py3.13_env/bin/python3 -m esptool"
  fi
fi

flash() {  # flash <offset> <file> [<offset> <file> ...]
  eval $ESPTOOL --chip esp32s3 -p "$PORT" -b 921600 \
    --before default-reset --after hard-reset write-flash \
    --flash-mode dio --flash-size 16MB "$@"
}

check_artifacts() {
  for f in bootloader/bootloader.bin partition_table/partition-table.bin \
           ota_data_initial.bin 01_factory.bin; do
    if [ ! -f "$ART/$f" ]; then
      echo "❌ 缺少 $ART/$f"
      echo "   先运行: python3 scripts/build.py"
      exit 1
    fi
  done
}

case "$MODE" in
  app)
    check_artifacts
    echo "正在向 $PORT 烧录应用分区（保留 Wi-Fi 配网与凭据）..."
    flash 0x20000 "$ART/01_factory.bin"
    ;;

  merged)
    [ -f "$MERGED" ] || { echo "❌ 找不到 $MERGED"; exit 1; }
    echo "⚠️  合并包从 0x0 开始，会覆盖 nvs：配网、凭据、设置全部清空。"
    echo "正在向 $PORT 烧录 $MERGED ..."
    flash 0x0 "$MERGED"
    ;;

  erase)
    check_artifacts
    echo "⚠️  恢复出厂：先整片擦除，再写入固件。配网与凭据会丢失。"
    eval $ESPTOOL --chip esp32s3 -p "$PORT" -b 921600 erase-flash
    echo "正在写入 bootloader / 分区表 / ota_data / app ..."
    flash \
      0x0    "$ART/bootloader/bootloader.bin" \
      0x8000 "$ART/partition_table/partition-table.bin" \
      0xd000 "$ART/ota_data_initial.bin" \
      0x20000 "$ART/01_factory.bin"
    ;;

  full)
    if [ -f "$ART/01_factory.bin" ]; then
      echo "正在向 $PORT 更新整机固件（保留 Wi-Fi 配网与凭据）..."
      flash \
        0x0    "$ART/bootloader/bootloader.bin" \
        0x8000 "$ART/partition_table/partition-table.bin" \
        0xd000 "$ART/ota_data_initial.bin" \
        0x20000 "$ART/01_factory.bin"
    else
      [ -f "$MERGED" ] || { echo "❌ 找不到 $MERGED"; exit 1; }
      echo "⚠️  没有编译产物，退回刷合并包：会清空 nvs（配网与凭据丢失）。"
      echo "    为避免丢配网，先运行 python3 scripts/build.py 再用 ./flash.sh。"
      flash 0x0 "$MERGED"
    fi
    ;;
esac

echo "✅ 烧录完成！设备已自动重启。"
