#!/usr/bin/env python3
"""
CubePaw / Yumo OS Build Script
Automates Ninja compilation for ESP32-S3 Touch LCD 1.54
"""
import os, stat, subprocess, sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
WORKSPACE = REPO_ROOT / "yumo-esp32-os"
ROOT = WORKSPACE / "firmware"
BUILD = WORKSPACE / "artifacts/firmware"
GIT_WRAPPER = SCRIPT_DIR / "bin/git"
GIT_WRAPPER.chmod(GIT_WRAPPER.stat().st_mode | stat.S_IEXEC)
SDK = Path("/Users/rebuild/esp/esp-idf")
TOOLS = Path("/Users/rebuild/.espressif")
PY = Path("/Users/rebuild/.espressif/python_env/idf6.1_py3.13_env/bin/python3")
CMAKE = Path("/Users/rebuild/.espressif/python_env/idf6.1_py3.13_env/lib/python3.13/site-packages/cmake/data/bin/cmake")
NINJA = Path("/Users/rebuild/.espressif/python_env/idf6.1_py3.13_env/bin/ninja")
XTENSA = Path("/Users/rebuild/.espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin")

env = os.environ.copy()
env.update(
    IDF_PATH=str(SDK),
    IDF_TOOLS_PATH=str(TOOLS),
    IDF_COMPONENT_MANAGER="1",
    ESP_IDF_VERSION="6.1",
    PYTHONUTF8="1",
    PYTHONDONTWRITEBYTECODE="1",
    PYTHONNOUSERSITE="1",
    CCACHE_DISABLE="1",
    GIT_TERMINAL_PROMPT="0",
    IDF_SKIP_CHECK_SUBMODULES="1",
    IDF_COMPONENT_MANAGER_CACHE_PATH=str(BUILD / "Espressif/ComponentManager/Cache"),
)
env["PATH"] = os.pathsep.join([str(PY.parent), str(CMAKE.parent), str(NINJA.parent), str(XTENSA), env.get("PATH", "")])

BUILD.mkdir(parents=True, exist_ok=True)

if not (BUILD / "build.ninja").is_file():
    print("=== Configuring CMake ===", flush=True)
    cmd_cfg = [
        str(CMAKE),
        "-S", str(ROOT),
        "-B", str(BUILD),
        "-G", "Ninja",
        "-DESP_PLATFORM=1",
        "-DIDF_TARGET=esp32s3",
        "-DCCACHE_ENABLE=0",
        f"-DGIT_EXECUTABLE={GIT_WRAPPER}",
        f"-DPYTHON={PY}",
        f"-DSDKCONFIG={ROOT / 'sdkconfig'}",
        f"-DCMAKE_MAKE_PROGRAM={NINJA}",
    ]
    subprocess.run(cmd_cfg, cwd=ROOT, env=env, check=True)

print("=== Building with Ninja ===", flush=True)
subprocess.run([str(NINJA), "-C", str(BUILD)], env=env, check=True)

# Update merged bin at root
print("=== Updating boba-os-merged.bin ===", flush=True)
merged_bin = REPO_ROOT / "boba-os-merged.bin"
cmd_merge = [
    str(PY), "-m", "esptool", "--chip", "esp32s3", "merge-bin",
    "-o", str(merged_bin),
    "--flash-mode", "dio",
    "--flash-size", "16MB",
    "0x0", str(BUILD / "bootloader/bootloader.bin"),
    "0x8000", str(BUILD / "partition_table/partition-table.bin"),
    "0xd000", str(BUILD / "ota_data_initial.bin"),
    "0x20000", str(BUILD / "01_factory.bin"),
]
subprocess.run(cmd_merge, env=env, check=True)
print("=== Build & Merge Complete! ===", flush=True)
