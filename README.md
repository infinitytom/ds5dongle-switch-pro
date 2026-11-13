# DS5 Dongle · Switch Pro

让 DualSense / DualSense Edge 通过 **LCTech BL616 接收器**连接主机，并在 DualSense、Xbox 360 / XInput 与 Switch Pro USB 模式之间切换。

基于 [sqlCRT/ds5dongle-bl618-opensource](https://github.com/sqlCRT/ds5dongle-bl618-opensource) 的实验性扩展，新增 Switch Pro 协议、HD 震动波形转译和完全离线的本地配置页。保留上游历史与 GPL-3.0 许可。

> **实验预发行版，已通过部分 Windows 实机测试。** LCTech BL616 的 Pro USB 握手、SDL3 实际打开与 Pro+麦克风音频采集已通过；Switch 1/2 枚举、完整按键、真实体感和触觉效果仍待验证。请确认板型并保留原固件，再自行进行硬件测试。

## 下载与使用

从 [Releases](https://github.com/infinitytom/ds5dongle-switch-pro/releases) 下载：

- `ds5dongle-switch-pro-lctech616-fs-*.zip`：LCTech BL616 Full-Speed 实验固件、boot2、分区文件、刷机说明与校验值。
- `ds5dongle-local-config-*.zip`：电脑离线配置工具，包含赞赏码，不依赖在线配置网站。
- 发布页的 Source code：与该版本对应的完整项目源码。

本地配置需要 **Python 3 + 桌面 Chrome / Edge**。解压配置工具后双击 `start-config.cmd`，点击「连接接收器」，选择 USB 接收器。Linux/macOS 可执行 `python3 web/serve.py`。

启用 Switch Pro 需刷入 **alpha.3 或更新固件**，再在配置页选择 **Switch Pro → 保存并应用**。页面等待闪存确认后会自动重启接收器、尝试重新连接；浏览器首次见到新 USB 身份时，按提示授权。主机端需开启「Pro 手柄的有线通信」，通过底座 USB 接入。Pro 模式保留本地配置接口，可以切回 DualSense 模式。

兼容进度：[平台兼容表](docs/COMPATIBILITY_CN.md)；详细步骤：[中文使用说明](docs/SWITCH_PRO_CN.md) · [验证记录](docs/VALIDATION.md) · [上游说明](README_UPSTREAM.md)

`v0.1.0-alpha.2` 是启动器编码补丁；`alpha.3` 包含 USB 固件修复，需更新配置包并刷入新固件。

**alpha.3 需要更新固件**：已在旧固件上复现 SDL 打开时报 `Couldn't setup USB mode`，定位到 BL616 SDK 对同编号 IN/OUT 端点使用同一 FIFO。新版使用 **IN 0x84 / OUT 0x01**，并增加标准 0x3F 输入报告、可选 USB 麦克风、实时诊断和重新设计的控制台。2026-11-06 已在新固件上通过 Windows USB 握手、SDL3 打开及麦克风真实采集，详见验证记录。

**alpha.4** 进一步将 Pro 模式的网页设备选择限定到厂商配置接口（Usage Page `0xFF00`），避免误选到不能读配置的游戏手柄接口。其固件与 alpha.3 相同，版本标记仍为 `Pro-dev1 r3`。

**alpha.5 / r4 本地测试版**：恢复 Xbox 360 / XInput 输出（模式 5），保留旧模式编号和配置布局。提供模拟扳机、双摇杆、A/B/X/Y、Guide 与双马达震动；独立厂商 HID 接口用于网页配置及切回其他模式。Xbox 模式不提供音频、体感、自适应扳机或 Xbox 主机认证。需要刷入 r4 固件并使用同版配置页；r3 固件会被页面阻止写入模式 5。本轮完成软件验证，尚未刷写和验证 Xbox 实机效果。

**alpha.6 / r5 本地测试版**：模式 5 补上 XInput 与四通道 USB 音频/触觉并存，使用复合设备测试身份 `1209:DB05`；保留 r4 身份的网页连接兼容。新增独立的 **Switch 2 Pro 实验模式 6**，包含 SDL 风格初始化、输入、体感与单帧基础震动转换，未提供耳麦音频或完整 HD Rumble 2。现有 Pro+麦克风保持模式 4。模式与主机兼容不是一回事，详见[平台兼容表](docs/COMPATIBILITY_CN.md)。本轮不承诺 PS / Xbox 主机认证或 Switch 2 主机原生支持。

**alpha.7 / r6**：本地控制台文字、按钮和状态数字加大加粗；修复 Pro 2 默认 `0x09` 的逐键位序、HID 按钮索引和浏览器四轴布局，`0x05` 保留原 Nintendo 坐标约定。更新页面即可使用新界面，Pro 2 修复需要重新刷入 r6。刷后拔插接收器并刷新测试网站；本轮没有自动刷写或主机实测。

**alpha.12 / r11（电量灯与 Pro2 扩展）**：任系三种模式的默认白色配置改为灯条自动显示电量；自定义 RGB 保持固定色。页面补充低电量与等待连接的闪灯说明，修正异常电量显示。接收器红紫交替不能单凭灯色判断掉线，本次尚无故障时诊断，未宣称掉线已修复。

**alpha.11 / r10（Pro2 本地测试版）**：新增主机 `09` 原生体感、Edge 背键、玩家灯、校准分片读写与持久化路径，兼容已知格式的主机短震动包。参考有真实 NS2 体感验证的新版 PicoSwitch2；本接收器尚未实测，耳麦、完整 HD Rumble 2、NFC 和唤醒仍未完成。见 [Pro2 功能清单](docs/PRO2_FEATURES_CN.md)。

**alpha.10 / r9（断线排查测试版）**：纯 Pro / Pro＋麦克风的触觉发送堵塞改为退避重试，避免仅输出失败就主动断开整个手柄。连接诊断新增最近断线原因和断线前电量（需 r9，历史仅在 RAM）。用户本次掉线原因尚未复现，r9 未完成实机长测；详见验证记录。

**alpha.9 / r8（主机握手测试版）**：用户反馈 r7 任系模式无法在 NS2 识别。现参考有真实主机记录的 PicoSwitch2 / GP2040-CE 补充 Pro 启动通知与 Pro2 原生身份、USB 配对和初始化；目标为 NS1 纯 Pro，NS2 三种任系模式。BL616 接收器仍待主机验收；Pro2 的主机 0x09 体感与完整 HD Rumble 2 未完成。详见 [主机协议对照](docs/NS2_CONSOLE_RESEARCH_CN.md)。

**alpha.8 / r7（龙神体感修复）**：已在 r6 实机复现龙神 HD 初始化的 `80 05` 被错误解释为退出完整报告，从 `0x30` 变成不含 IMU 的 `0x3F`。r7 保留完整报告和 IMU 状态，并新增连接诊断页的实时体感与限时 HD 试振。此修复适用于纯 Pro / Pro+麦克风；Pro 2 继续为实验协议，不代表完整 HD Rumble 2。需要刷入 r7，重新在龙神选择当前 SDL 设备后进游戏验证，不能只更新网页。

## 当前功能

| 功能 | 实现与限制 |
|---|---|
| USB 模式 | DualSense / Edge / 自动、Xbox+音频（1209:db05）、Switch Pro、Pro+麦克风（057e:2009）、Pro 2 实验（057e:2069） |
| 按键与摇杆 | Switch Pro 12 位摇杆、方向键、HOME、Capture、电量；两组按键映射 |
| 体感 | 双方 IMU 单位与坐标系转换、DualSense 工厂校准、Pro 量程设置；每报告重复当前样本三次 |
| HD 震动 | 左右高低频参数解码 → 连续相位 3 kHz 波形 → 上游 DualSense 蓝牙触觉通道 |
| 本地配置 | 控制台、实时按键/摇杆、电量/信号、自动保存重连、映射、音频电平测试和诊断导出 |
| 麦克风 | Pro + USB 麦克风模式提供独立 UAC1 2ch/48kHz 输入，单声道蓝牙麦克风复制到双声道；Windows 实际采集通过，Switch 兼容性待测 |
| 停止处理 | 震动超时、禁用、USB 挂起/复位与手柄断线处理 |

按实体位置映射：**○→A、×→B、△→X、□→Y**；Create→−、Options→+、PS→HOME、触摸板按下→Capture。L2/R2 转为数字 ZL/ZR，不保留模拟行程。

当前只支持传统 Switch Pro 的 4 字节/通道震动编码。模拟器必须输出完整 HD 震动参数；普通强弱震动后端无法恢复原始 HD 细节。DualSense 与 Nintendo 执行器的物理差异仍需调校。

不提供 NFC/amiibo、红外、无线连接 Switch、主机唤醒、Switch 2 Pro 的 C/GL/GR 按键或 HD Rumble 2。纯 Pro 不暴露音频接口；Pro + USB 麦克风模式只增加音频输入。需要扬声器、耳机输出、自适应扳机和键鼠功能时请使用 DS/Edge 模式。独立 USB 麦克风的 Switch 2 兼容性尚未验证，它不是对 Switch 2 Pro 私有音频传输的实现。

协议参考：[DS5_Bridge Xbox 实现](https://github.com/SundayMoments/DS5_Bridge/tree/main/src/persona)。本分支使用独立 C 实现与 CherryUSB 传输层。

## 构建和测试

必须使用上游的 [专用 Bouffalo SDK](https://github.com/sqlCRT/bouffalo_sdk)，本版本基于提交 `b3532953322194233f80436701e7fb6bb4ad8275`，以及 [T-Head Windows 工具链](https://gitee.com/bouffalolab/toolchain_gcc_t-head_windows)。完整命令见[构建说明](docs/SWITCH_PRO_CN.md#构建)。SDK 和工具链属于外部依赖，不随本仓库内置。

```sh
# 配置协议与网页测试
npm ci
npm test

# 协议核心测试（Linux/macOS，需 C 编译器）
mkdir -p build-tests
cc -std=c11 -Wall -Wextra -Werror -Isrc tests/switch_pro_test.c src/switch_pro.c src/switch_usb.c -lm -o build-tests/switch-pro-test
./build-tests/switch-pro-test
```

Windows 可运行 `tests\run-native.cmd`（Visual Studio C++ Build Tools）。浏览器回归使用 Playwright 和模拟 WebHID，不能替代实机测试。

## 支持开发

如果本分支对你有帮助，欢迎 Star 或自愿赞赏，支持后续开发和硬件测试。赞赏不影响功能使用。

<img src="web/assets/alipay-donate.jpg" alt="支付宝赞赏码，收款名称 infinitytom(*昊)" width="240">

支付宝收款名称：**infinitytom(*昊)**。这是本分支维护者的赞赏码；上游项目的贡献与权利仍归各自作者。

## 致谢与许可

- [sqlCRT/ds5dongle-bl618-opensource](https://github.com/sqlCRT/ds5dongle-bl618-opensource)：本分支基于其蓝牙、USB、音频与触觉实现。
- [awalol/DS5Dongle](https://github.com/awalol/DS5Dongle)：上游原始移植来源。
- [Nintendo Switch Reverse Engineering](https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering)：公开协议研究。
- [SDL](https://github.com/libsdl-org/SDL)：坐标转换与校准协议参考，未复制驱动实现。
- Bouffalo SDK、CherryUSB、Opus 及其他上游贡献者，见 [NOTICE](NOTICE)。

源代码遵循 [GPL-3.0](LICENSE)，第三方组件保留各自许可。发布修改后的产品时请同时提供对应源码。本项目与 Sony、Nintendo 无关联、未获其认可；相关名称与商标归各自权利人所有。

---

**English:** Experimental DualSense-to-Switch-Pro USB bridge for the LCTech BL616, with offline WebHID configuration and frequency-preserving haptic synthesis. Windows hardware tests pass for USB handshakes, SDL3 gamepad opening and microphone capture; full input, motion, haptics and Switch console compatibility remain unverified. This is an unofficial GPL-3.0 fork; upstream attribution is preserved.


本地生成包含准确源码快照的测试包（不提交、不发布、不刷写）：`py -3 tools/package_release.py --local-preview`。正式打包仍要求工作区已提交。

### 组合键切换与自动连接（alpha.13 / r12）

同时长按 **PS + Options + 下表按键 2 秒**，直接切换到指定模式：

| 按键 | 目标模式 |
|---|---|
| × | DS5 |
| ○ | DualSense Edge |
| □ | Auto |
| △ | Switch Pro |
| 方向键 ↑ | Pro + USB 麦克风 |
| 方向键 → | Xbox + 音频 |
| 方向键 ↓ | Pro 2（实验） |

仅按 PS + Options 不切换。一次只选一个目标；改变目标按键会重新计时。保存成功后自动重启，失败则保持原模式；选择当前模式不保存、不重启。再次触发前需松开 PS 和 Options；上电或输入中断后也需先松手。网页连接后可查看实际模式。

网页打开、接收器重新插入或组合键切换后，自动连接已授权的配置接口。首次连接以及新的 USB 模式身份仍需点击连接按钮授权。多接收器时自动连接第一个可用接口，也可点击连接按钮手动选择。
