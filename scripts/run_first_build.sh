#!/usr/bin/env bash
set -e

PROJECT_ROOT="/Users/rebuild/Documents/nico/ai_project/CubePaw"
BUILD_DIR="${PROJECT_ROOT}/.build_xiaozhi"
mkdir -p "${BUILD_DIR}"

echo "=========================================="
echo " [1/3] 激活 ESP-IDF v6.1 本地编译环境..."
echo "=========================================="
source "${HOME}/esp/esp-idf/export.sh"

echo "=========================================="
echo " [2/3] 准备 xiaozhi-esp32 开源源码..."
echo "=========================================="
cd "${BUILD_DIR}"
if [ ! -d "xiaozhi-esp32/.git" ]; then
    echo "正在拉取 xiaozhi-esp32 主仓库..."
    git clone --depth 1 https://ghproxy.net/https://github.com/78/xiaozhi-esp32.git xiaozhi-esp32 2>/dev/null || \
    git clone --depth 1 https://github.com/78/xiaozhi-esp32.git xiaozhi-esp32
fi

cd xiaozhi-esp32
echo "源码最新 Commit:"
git log -1 --oneline

echo "=========================================="
echo " [3/3] 检查板型并执行编译测试..."
echo "=========================================="
# 列出微雪 1.54寸板型的具体路径
ls -d main/boards/*1.54* || true

# 设置芯片目标
idf.py set-target esp32s3

echo "=========================================="
echo " 基线环境就绪！"
echo "=========================================="
