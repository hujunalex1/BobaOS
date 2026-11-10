#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_WORKSPACE="${PROJECT_ROOT}/.build_xiaozhi"

echo "========================================================"
echo " CubePaw OS (小玉掌上多合一系统) 一键固件构建脚本"
echo " 目标板型: Waveshare ESP32-S3-Touch-LCD-1.54 (240x240)"
echo "========================================================"

# 1. 克隆或更新 xiaozhi-esp32 主干代码
if [ ! -d "${BUILD_WORKSPACE}/xiaozhi-esp32" ]; then
    mkdir -p "${BUILD_WORKSPACE}"
    echo "[1/4] 正在拉取 xiaozhi-esp32 开源仓库..."
    git clone --depth=1 https://github.com/78/xiaozhi-esp32.git "${BUILD_WORKSPACE}/xiaozhi-esp32"
fi

# 2. 注入 CubePaw OS 组件源码
echo "[2/4] 正在注入 CubePaw OS 6大功能模块与防回声自激补丁..."
mkdir -p "${BUILD_WORKSPACE}/xiaozhi-esp32/components/cubepaw_os"
cp -R "${PROJECT_ROOT}/firmware/"* "${BUILD_WORKSPACE}/xiaozhi-esp32/components/cubepaw_os/"

# 3. 使用 ESP-IDF 或 Docker 编译固件
echo "[3/4] 正在编译 ESP32-S3 固件..."
if command -v idf.py >/dev/null 2>&1; then
    cd "${BUILD_WORKSPACE}/xiaozhi-esp32"
    idf.py set-target esp32s3
    idf.py build
    python3 scripts/release.py esp32-s3-touch-lcd-1.54
elif command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
    docker run --rm -v "${BUILD_WORKSPACE}/xiaozhi-esp32:/project" -w /project espressif/idf:release-v5.4 \
        bash -c "idf.py set-target esp32s3 && idf.py build"
else
    echo "提示: 本地尚未启动 Docker 或安装 ESP-IDF，可直接使用 GitHub Actions 云端编译，或启动 Docker Desktop 后重新执行本脚本。"
    exit 0
fi

# 4. 自动注入小玉 21 张定制表情到最终固件
echo "[4/4] 正在打包小玉 21 张手绘表情到 Assets 分区..."
python3 "${PROJECT_ROOT}/scripts/make_xiaoyu_emojis.py"
echo "构建完成！"
