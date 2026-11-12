#!/usr/bin/env python3
"""
CubePaw / Yumo OS Build Script
Automates Ninja compilation for ESP32-S3 Touch LCD 1.54
"""
import os, stat, subprocess, sys
from pathlib import Path


def find_idf_path() -> Path:
    if os.environ.get("IDF_PATH"):
        return Path(os.environ["IDF_PATH"]).expanduser().resolve()

    candidates = [
        Path.home() / "esp" / "esp-idf",
        Path("/opt/esp-idf"),
        *Path.home().glob(".espressif/frameworks/esp-idf-*"),
    ]
    for candidate in candidates:
        if (candidate / "export.sh").is_file():
            return candidate.resolve()

    raise RuntimeError("未找到 ESP-IDF，请设置 IDF_PATH 环境变量")


def find_python_env(tools_path: Path) -> Path:
    envs = sorted(tools_path.glob("python_env/idf*/bin/python3"))
    if not envs:
        raise RuntimeError(f"未找到 ESP-IDF Python 环境，请检查 {tools_path}/python_env")
    return envs[-1]


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
WORKSPACE = REPO_ROOT / "yumo-esp32-os"
ROOT = WORKSPACE / "firmware"
BUILD = WORKSPACE / "artifacts/firmware"
GIT_WRAPPER = SCRIPT_DIR / "bin/git"
GIT_WRAPPER.chmod(GIT_WRAPPER.stat().st_mode | stat.S_IEXEC)
SDK = find_idf_path()
TOOLS = Path(os.environ.get("IDF_TOOLS_PATH", "~/.espressif")).expanduser().resolve()
PY = find_python_env(TOOLS)
CMAKE = next((PY.parent.parent / "lib").glob("python*/site-packages/cmake/data/bin/cmake"))
NINJA = PY.parent / "ninja"
TOOLCHAINS = sorted((TOOLS / "tools" / "xtensa-esp-elf").glob("*/xtensa-esp-elf/bin"))
XTENSA = TOOLCHAINS[-1] if TOOLCHAINS else None

if not CMAKE.is_file():
    raise RuntimeError(f"未找到 CMake: {CMAKE}")
if not NINJA.is_file():
    raise RuntimeError(f"未找到 Ninja: {NINJA}")
if XTENSA is None:
    raise RuntimeError(f"未找到 Xtensa 工具链，请检查 {TOOLS}/tools/xtensa-esp-elf")

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
