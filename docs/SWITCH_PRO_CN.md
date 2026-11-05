# DualSense 接收器：Switch Pro 实验分支

本目录是对 sqlCRT/ds5dongle-bl618-opensource 的本地修改，源代码仍按 GPL-3.0 发布。上游原始提交可用 `git rev-parse HEAD` 查看；本地变更用 `git diff` 查看。没有修改或刷写已经连接的设备。

## 使用本地配置页

1. 双击根目录 `start-config.cmd`，或执行 `powershell -ExecutionPolicy Bypass -File .\start-config.ps1` / `C:\Python313\python.exe web/serve.py`。
2. 用桌面 Chrome / Edge 打开 <http://localhost:8765>，通过 USB 接上接收器，点击「连接接收器」。浏览器中选择带接收器配置接口的设备。
3. 已有开源固件可以读取和修改 DS/Edge 设置。切换到 Switch Pro 需要先刷入这个分支编译的、版本号含 `Pro-dev1` 的固件；页面会阻止向旧固件写入模式 3。
4. 修改模式后点击「保存并重启」，重新授权枚举后的设备。Pro 模式也保留专用配置 Feature Reports，因此能再切回 DS 模式。

页面无 CDN、在线字体、遥测和服务器 API，网络断开仍可使用。Python 服务器只监听 127.0.0.1。重启电脑后重新运行脚本即可。不要直接双击 HTML（ES modules 与 WebHID 需要本地 HTTP 安全上下文）。如果已有服务器占用 8765，直接打开上述地址。

原作者配置网页没有开源；这里是根据固件协议独立实现的本地工具，不是复制原站源码，也不提供在线 OTA。

## 协议与功能

| 项目 | 实现 |
|---|---|
| 模式 | `controller_mode=3`，沿用 version 2 的 29 字节配置布局 |
| USB 身份 | 实验性模拟 057e:2009 Pro Controller；单个 HID 接口，IN 0x81 / OUT 0x01，64 字节报告 |
| 握手 | 0x80 的 identify、handshake、baudrate、USB timeout 命令 |
| 输入 | 0x30，8 ms 周期；按键、12 位双摇杆、电量、三组 IMU 数据 |
| 主机命令 | 0x01 子命令，0x21 ACK；只读 SPI 工厂校准、输入模式、IMU、振动和玩家灯状态 |
| HD 震动 | 0x01 / 0x10 内左右两组 4 字节传统频率/振幅编码；分别解码高低频，连续相位合成 3 kHz 立体声 int8 波形 |
| 触觉输出 | 使用上游已存在的蓝牙 0x39 / 0xD2 双帧通道，每包 64 对采样，约 21.33 ms；Pro 模式不需要主机 USB Audio 流 |
| 停止保护 | 禁用振动、USB 复位/挂起、或 250 ms 没有新振动参数时输出静音 |
| 本地配置 | F6 写配置/保存/重启，F7 读配置，F8 版本，F9 状态，FB 两组按键映射 |

默认按实体位置映射：○→A，×→B，△→X，□→Y；L1/R1→L/R，L2/R2→ZL/ZR，Create→−，Options→+，PS→HOME，触摸板按下→Capture。扳机使用 DualSense 的数字按下状态，转换后不保留模拟行程。已有 remap 表先应用，再转换为 Switch 布局。

体感从 DualSense 原始 IMU 转换为 Pro 单位；收到真实 DualSense 校准报告后应用其偏移和比例，再按 `Pro=(-DS.z,-DS.x,DS.y)` 转换两类传感器坐标。支持量程子命令 0x41；没有校准时使用名义参数。每份 Pro 报告内重复当前 IMU 样本三次，尚未实现历史三样本重采样，方向和比例仍需实机校验。SPI 返回的是与输出单位匹配的合成 Pro 工厂参数，不是假装读取了 Nintendo 硬件校准。

## 主机与模拟器

Switch 1 / 2：通过底座 USB 连接接收器，在主机设置中启用「Pro 手柄的有线通信」。本分支使用一代 Pro 协议；任天堂对原装一代 Pro 的支持不代表本模拟固件已经通过兼容测试。

电脑模拟器：选择原生 Switch Pro / SDL HIDAPI 输入后端。模拟器必须输出完整 HD 震动编码，才能保留频率变化；只输出普通强弱震动的后端无法还原原始 HD 细节。Steam Input 等中间层可能改变输出路径。不要同时让多个程序抢占接收器。

目前不提供 NFC / amiibo、红外、无线连接主机、主机唤醒、Switch 2 Pro 的 C/GL/GR 按键、HD Rumble 2。Pro 模式不暴露原来的 USB 声卡和键鼠接口，原有音频、麦克风、自适应扳机和键鼠映射功能请切回 DS/Edge 模式使用。震动是波形转译，DualSense 执行器与 Nintendo 执行器的物理差异需要后续调校，不能保证手感一致。

## 构建

依赖是上游专用 SDK 与 T-Head Windows 工具链，不能用官方 SDK 或普通 RISC-V GCC 替代。本机预期路径：

```powershell
$env:BL_SDK_BASE = 'E:\bouffalo_sdk'
$env:TOOLCHAIN_PATH = 'E:\toolchain_gcc_t-head_windows'
$env:BOARD_TYPE = 'lctech616' # aim61 / m0sdock 按实际硬件修改
$env:USB_SPEED = 'fs'
$env:GIT_NO_LAZY_FETCH = '1' # 本机精简 SDK 避免版本检查触发额外下载
.\build_windows.bat rebuild
```

先使用默认 Full-Speed。本次已完成 LCTech BL616 Full-Speed 编译、链接、DWARF 处理与镜像打包，产物位于 `firmware/lctech616/ds5dongle-lctech616.bin`。其他板型和 High-Speed 版本尚未构建。

Windows 构建脚本加入 `python3` 启动适配（优先 `DS5_PYTHON`，其次 `py -3`，最后 `python`），并修复失败时仍返回成功的问题。本机 SDK 从固定提交 `b3532953322194233f80436701e7fb6bb4ad8275` 按需取得；它是构建所需子集，不是完整 SDK 检出。

不要把其他板型的产物刷入自己的硬件。完整烧写需要匹配的 boot2、分区表和固件文件，流程见上游 README_CN.md。保留你现有的可用固件和分区配置以便恢复。

## 验证

```powershell
.\tests\run-native.cmd   # 本机 Visual C++，协议核心 / 握手 / 映射 / IMU / 震动 / 畸形报文
node --test tests/web.test.mjs
# 安装 Playwright 时，设置 PLAYWRIGHT_MODULE 或让 Node 能找到 playwright：
node tests/browser.cjs  # 本地 8765 服务器需已启动，使用 headless Edge + 模拟 WebHID
```

浏览器测试使用模拟设备，覆盖读取带填充的报告、写入回读、映射、移动端布局；截图 `docs/local-config.png` 中的设备是测试数据。测试通过不能替代硬件验证。

待实机逐项记录：Windows 识别与切回 DS、Switch 1/2 底座枚举、所有按键和摇杆中心、IMU 静止漂移与轴向、左右震动隔离和频率扫描、停止振动、USB 拔插/挂起/复位、手柄断线重连、连续运行至少 30 分钟。保存主机系统版本、板型、固件 SHA256、模拟器名称/版本及 HID 抓包，便于定位兼容问题。

## 协议参考

- 上游固件与许可：<https://github.com/sqlCRT/ds5dongle-bl618-opensource>
- 专用 SDK：<https://github.com/sqlCRT/bouffalo_sdk>
- Nintendo USB 协议：<https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/USB-HID-Notes.md>
- 子命令：<https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/bluetooth_hid_subcommands_notes.md>
- 频率/振幅编码：<https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/blob/master/rumble_data_table.md>
- DualSense 校准单位：<https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_ps5.c>
- Switch 2 原装一代 Pro 有线设置：<https://support-jp.nintendo.com/app/answers/detail/a_id/38800/>
