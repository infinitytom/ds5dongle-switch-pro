# DS5 Dongle · Switch Pro

让 DualSense / DualSense Edge 通过 **LCTech BL616 接收器**连接主机，并在 DualSense 与 Switch Pro USB 模式之间切换。

基于 [sqlCRT/ds5dongle-bl618-opensource](https://github.com/sqlCRT/ds5dongle-bl618-opensource) 的实验性扩展，新增 Switch Pro 协议、HD 震动波形转译和完全离线的本地配置页。保留上游历史与 GPL-3.0 许可。

> **实验预发行版，尚未实机验证。** 已完成软件测试和 LCTech BL616 Full-Speed 固件构建；不能据此保证 Switch 1/2 枚举、真实体感和触觉效果。请确认板型并保留原固件，再自行进行硬件测试。

## 下载与使用

从 [Releases](https://github.com/infinitytom/ds5dongle-switch-pro/releases) 下载：

- `ds5dongle-switch-pro-lctech616-fs-*.zip`：LCTech BL616 Full-Speed 实验固件、boot2、分区文件、刷机说明与校验值。
- `ds5dongle-local-config-*.zip`：电脑离线配置工具，包含赞赏码，不依赖在线配置网站。
- 发布页的 Source code：与该版本对应的完整项目源码。

本地配置需要 **Python 3 + 桌面 Chrome / Edge**。解压配置工具后双击 `start-config.cmd`，点击「连接接收器」，选择 USB 接收器。Linux/macOS 可执行 `python3 web/serve.py`。

启用 Switch Pro 需先刷入本分支固件，再在配置页选择 **Switch Pro（实验）→ 保存并重启**。主机端需开启「Pro 手柄的有线通信」，通过底座 USB 接入。Pro 模式保留本地配置接口，可以切回 DualSense 模式。

详细步骤：[中文使用说明](docs/SWITCH_PRO_CN.md) · [验证记录](docs/VALIDATION.md) · [上游说明](README_UPSTREAM.md)

## 当前功能

| 功能 | 实现与限制 |
|---|---|
| USB 模式 | DualSense / Edge / 自动，以及实验性 Switch Pro（057e:2009） |
| 按键与摇杆 | Switch Pro 12 位摇杆、方向键、HOME、Capture、电量；两组按键映射 |
| 体感 | 双方 IMU 单位与坐标系转换、DualSense 工厂校准、Pro 量程设置；每报告重复当前样本三次 |
| HD 震动 | 左右高低频参数解码 → 连续相位 3 kHz 波形 → 上游 DualSense 蓝牙触觉通道 |
| 本地配置 | WebHID 直连、读取/保存/重启、配置导入导出；所有页面资源随包提供 |
| 停止处理 | 震动超时、禁用、USB 挂起/复位与手柄断线处理 |

按实体位置映射：**○→A、×→B、△→X、□→Y**；Create→−、Options→+、PS→HOME、触摸板按下→Capture。L2/R2 转为数字 ZL/ZR，不保留模拟行程。

当前只支持传统 Switch Pro 的 4 字节/通道震动编码。模拟器必须输出完整 HD 震动参数；普通强弱震动后端无法恢复原始 HD 细节。DualSense 与 Nintendo 执行器的物理差异仍需调校。

不提供 NFC/amiibo、红外、无线连接 Switch、主机唤醒、Switch 2 Pro 的 C/GL/GR 按键或 HD Rumble 2。Pro 模式不暴露 USB 声卡和键鼠接口；需要原有音频、麦克风、自适应扳机和键鼠功能时请使用 DS/Edge 模式。

## 构建和测试

必须使用上游的 [专用 Bouffalo SDK](https://github.com/sqlCRT/bouffalo_sdk)，本版本基于提交 `b3532953322194233f80436701e7fb6bb4ad8275`，以及 [T-Head Windows 工具链](https://gitee.com/bouffalolab/toolchain_gcc_t-head_windows)。完整命令见[构建说明](docs/SWITCH_PRO_CN.md#构建)。SDK 和工具链属于外部依赖，不随本仓库内置。

```sh
# 配置协议与网页测试
npm ci
npm test

# 协议核心测试（Linux/macOS，需 C 编译器）
mkdir -p build-tests
cc -std=c11 -Wall -Wextra -Werror -Isrc tests/switch_pro_test.c src/switch_pro.c -lm -o build-tests/switch-pro-test
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

**English:** Experimental DualSense-to-Switch-Pro USB bridge for the LCTech BL616, with offline WebHID configuration and frequency-preserving haptic synthesis. Software tests and firmware builds pass; real controller/console compatibility is **not hardware verified**. This is an unofficial GPL-3.0 fork; upstream attribution is preserved.
