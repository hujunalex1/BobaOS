#!/usr/bin/env bash
set -e

ESP_DIR="${HOME}/esp"
IDF_PATH="${ESP_DIR}/esp-idf"
TOOLS_PATH="${ESP_DIR}/esp-gitee-tools"

echo "=========================================="
echo " 正在使用乐鑫官方 Gitee 国内加速源安装..."
echo " 安装路径: ${IDF_PATH}"
echo "=========================================="

mkdir -p "${ESP_DIR}"

# 1. 下载乐鑫官方国内子模块加速工具
if [ ! -d "${TOOLS_PATH}/.git" ]; then
    echo "[1/4] 克隆乐鑫国内加速工具 esp-gitee-tools..."
    git clone --depth 1 https://gitee.com/EspressifSystems/esp-gitee-tools.git "${TOOLS_PATH}"
fi

# 2. 从 Gitee 镜像拉取 esp-idf (release/v6.1)
if [ ! -d "${IDF_PATH}/.git" ]; then
    echo "[2/4] 从 Gitee 高速镜像克隆 esp-idf (release/v6.1)..."
    git clone -b release/v6.1 --depth 1 https://gitee.com/EspressifSystems/esp-idf.git "${IDF_PATH}"
fi

cd "${IDF_PATH}"

# 3. 使用加速工具更新子模块
echo "[3/4] 正在通过国内镜像加速拉取子模块..."
"${TOOLS_PATH}/submodule-update.sh"

# 4. 安装编译器工具链
echo "[4/4] 正在安装 esp32s3 交叉编译工具链与依赖..."
"${TOOLS_PATH}/install.sh" esp32s3

echo "=========================================="
echo " ESP-IDF v6.1 国内加速源安装配置完成！"
echo "=========================================="
