# 🧋 BobaOS 波波 OS

<p align="center">
  <img src="docs/preview.png" alt="BobaOS character previews" width="720">
</p>

**BobaOS** 是一个专为 **Waveshare ESP32-S3-Touch-LCD-1.54** 打造的儿童智能手表系统。项目基于开源的 [Yumo OS](./yumo-esp32-os) 二次开发，保留了稳定的硬件驱动与应用框架，并围绕儿童交互重新设计了桌面、动画、AI 助手和功耗策略。

## ✨ 核心亮点

- **草莓波波摇摇乐**：读取 QMI8658 六轴传感器，让 6 颗波波球随手表倾斜滚动、碰撞与回弹；摇一摇可触发炸开效果。
- **马卡龙糖果 UI**：白色圆角面板、粉色细描边与大号高频入口，面向儿童优化可视层级。
- **双页桌面**：第 1 页是小玩具与 AI、恐龙、萌芽物语入口；左滑进入应用抽屉。
- **表情系统**：内置多套角色表情资源，覆盖说话、开心、睡觉、困惑等状态。
- **AI 助手集成**：支持 DeepSeek API 配置、余额查询与对话界面，并包含移动端配置页。
- **省电策略**：熄屏、切换应用或进入后台时暂停物理引擎与传感器租约。

## 🧰 硬件要求

| 模块 | 规格 |
| --- | --- |
| 主控 | ESP32-S3，240 MHz 双核 |
| 存储 | 16 MB Flash + 8 MB PSRAM |
| 屏幕 | 1.54 英寸 ST7789V IPS，240 × 240 |
| 触摸 | CST816S 电容触摸 |
| 传感器 | QMI8658 六轴 IMU |
| 音频 | ES8311 DAC 与扬声器 |

## 🚀 烧录模式

仓库已内置完整固件 `boba-os-merged.bin`，不需要先安装 ESP-IDF。将手表通过 USB-C 连接电脑后，在仓库根目录运行对应命令；串口可自动检测，也可显式指定。

| 命令 | 用途 | NVS 数据 |
| --- | --- | --- |
| `./flash.sh` | 更新 Bootloader、分区表、OTA 数据和应用；缺少编译产物时退回刷合并包 | 保留 Wi-Fi 配网、凭据和设置 |
| `./flash.sh --app` | 只更新应用分区，速度最快 | 保留 |
| `./flash.sh --erase` | 恢复出厂：整片擦除后重写固件 | 清空 |
| `./flash.sh --merged` | 直接刷根目录合并包 | 清空，等价恢复出厂 |

默认模式会保留 `0x9000-0xD000` 的 NVS 分区。只有在需要彻底恢复出厂，或明确接受清空配网和凭据时，才使用 `--erase` 或 `--merged`。

显式指定串口示例：

   ```bash
   ./flash.sh --app /dev/cu.usbmodem2101
   ```

## 💻 源码构建

如需修改源码，请安装 **ESP-IDF 6.1**。构建脚本优先读取 `IDF_PATH`，否则会探测常见安装位置；Python 环境和工具链从 `IDF_TOOLS_PATH`（默认 `~/.espressif`）查找。然后在根目录执行：

```bash
python3 scripts/build.py
```

构建脚本会配置 CMake 工程、并行编译固件，并重新生成合并固件 `boba-os-merged.bin`。

## 📁 目录结构

```text
.
├── boba-os-merged.bin        # 可直刷的合并固件
├── flash.sh                  # 一键烧录脚本
├── scripts/build.py          # 一键构建与固件打包
├── docs/                     # 硬件、烧录与模型/语音说明
├── assets/                   # 角色、表情与 UI 图标
├── configs/                  # 板级与驱动配置
└── yumo-esp32-os/            # 系统源码
    ├── firmware/main/ui      # 桌面与界面逻辑
    ├── firmware/main/apps    # 内置应用
    └── firmware/main/core    # 传感器、电源与系统核心
```

## 📚 更多文档

- [硬件说明](docs/01_hardware_overview.md)
- [macOS 烧录指南](docs/02_flash_guide_mac.md)
- [模型与语音配置](docs/03_model_and_voice_guide.md)
- [Yumo OS 说明](yumo-esp32-os/README.md)

## 🙏 致谢

感谢 [Yumo OS](./yumo-esp32-os) 原作者提供扎实的 ESP32-S3 硬件生态基础，也感谢 [LVGL](https://lvgl.io/) 提供的开源图形引擎。

## 📄 许可证

本项目基于 **Apache 2.0** 开源。
