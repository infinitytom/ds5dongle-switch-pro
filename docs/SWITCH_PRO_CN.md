# DualSense 接收器：Switch Pro 实验分支

本目录是对 sqlCRT/ds5dongle-bl618-opensource 的本地修改，源代码仍按 GPL-3.0 发布。上游原始提交可用 `git rev-parse HEAD` 查看；本地变更用 `git diff` 查看。没有修改或刷写已经连接的设备。

## 使用本地配置页

1. 双击根目录 `start-config.cmd`，或执行 `powershell -ExecutionPolicy Bypass -File .\start-config.ps1` / `C:\Python313\python.exe web/serve.py`。
2. 用桌面 Chrome / Edge 打开 <http://localhost:8765>，通过 USB 接上接收器，点击「连接接收器」。浏览器中选择带接收器配置接口的设备。
3. 已有开源固件可以读取和修改 DS/Edge 设置。Switch Pro 需要 **alpha.3 固件（版本字符串含 Pro-dev1 r3）**；旧版本有已复现的 USB 收发端点冲突，控制台会提示升级。
4. 修改模式后点击「保存并应用」。页面等待闪存写入确认，再自动重启并尝试连接新身份。首次使用该 USB 身份时点击「授权新模式接收器」。USB 身份改变需要重新枚举，无需另外手动按重启。Pro 模式保留配置 Feature Reports，可以切回 DS 模式。

页面无 CDN、在线字体、遥测和服务器 API，网络断开仍可使用。Python 服务器只监听 127.0.0.1。重启电脑后重新运行脚本即可。不要直接双击 HTML（ES modules 与 WebHID 需要本地 HTTP 安全上下文）。如果已有服务器占用 8765，直接打开上述地址。

原作者配置网页没有开源；这里是根据固件协议独立实现的本地工具，不是复制原站源码，也不提供在线 OTA。

## 协议与功能

| 项目 | 实现 |
|---|---|
| 模式 | `controller_mode=3` 为 Pro，`4` 为 Pro+USB 麦克风；沿用 version 2 的 29 字节配置布局 |
| USB 身份 | 模拟 057e:2009；HID IN 0x84 / OUT 0x01，避免 BL616 同编号双向端点共用 FIFO |
| 握手 | 0x80 的 identify、handshake、baudrate、USB timeout 命令 |
| 输入 | 默认标准 0x3F（浏览器/普通 HID），主机可切到完整 0x30；8 ms 周期 |
| 主机命令 | 0x01 子命令，0x21 ACK；只读 SPI 工厂校准、输入模式、IMU、振动和玩家灯状态 |
| HD 震动 | 0x01 / 0x10 内左右两组 4 字节传统频率/振幅编码；分别解码高低频，连续相位合成 3 kHz 立体声 int8 波形 |
| 触觉输出 | 使用上游已存在的蓝牙 0x39 / 0xD2 双帧通道，每包 64 对采样，约 21.33 ms；Pro 模式不需要主机 USB Audio 流 |
| 停止保护 | 禁用振动、USB 复位/挂起、或 250 ms 没有新振动参数时输出静音 |
| 本地配置 | F6 写配置/保存/重启，F7 读配置，F8 版本，F9 状态，FB 两组按键映射 |
| Pro 麦克风 | 模式 4 增加 UAC1 接口 1/2，ISO IN 0x82，2ch/16bit/48kHz，支持主静音；有线复合设备兼容性待测 |

默认按实体位置映射：○→A，×→B，△→X，□→Y；L1/R1→L/R，L2/R2→ZL/ZR，Create→−，Options→+，PS→HOME，触摸板按下→Capture。扳机使用 DualSense 的数字按下状态，转换后不保留模拟行程。已有 remap 表先应用，再转换为 Switch 布局。

体感从 DualSense 原始 IMU 转换为 Pro 单位；收到真实 DualSense 校准报告后应用其偏移和比例，再按 `Pro=(-DS.z,-DS.x,DS.y)` 转换两类传感器坐标。支持量程子命令 0x41；没有校准时使用名义参数。每份 Pro 报告内重复当前 IMU 样本三次，尚未实现历史三样本重采样，方向和比例仍需实机校验。SPI 返回的是与输出单位匹配的合成 Pro 工厂参数，不是假装读取了 Nintendo 硬件校准。

## 主机与模拟器

Switch 1 / 2：通过底座 USB 连接接收器，在主机设置中启用「Pro 手柄的有线通信」。本分支使用一代 Pro 协议；任天堂对原装一代 Pro 的支持不代表本模拟固件已经通过兼容测试。

电脑模拟器：选择原生 Switch Pro / SDL HIDAPI 输入后端。模拟器必须输出完整 HD 震动编码，才能保留频率变化；只输出普通强弱震动的后端无法还原原始 HD 细节。Steam Input 等中间层可能改变输出路径。不要同时让多个程序抢占接收器。

目前不提供 NFC / amiibo、红外、无线连接主机、主机唤醒、Switch 2 Pro 的 C/GL/GR 按键、HD Rumble 2。纯 Pro 模式没有音频接口；模式 4 提供 USB 麦克风输入，不提供音频播放。原有扬声器、耳机输出、自适应扳机和键鼠功能请切回 DS/Edge 模式。震动是波形转译，执行器物理差异仍需调校。

控制台「音频与麦克风」可授权并选择系统音频输入，在本机观察电平，不录音或上传。请选择带 DualSense / Pro Controller 名称的输入设备，避免测到电脑内置麦克风。Switch 2 Pro 提供 3.5mm CTIA 耳麦接口，Switch 2 也支持外接 USB 麦克风；这里走独立 UAC1 USB 输入路线，不能据此保证主机接受本接收器的复合接口。任天堂说明：<https://support.nintendo.com/jp/switch2/accessory/controller/pro-controller/index.html>、<https://en-americas-support.nintendo.com/app/answers/detail/a_id/68398/>。

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


## alpha.5 / r4：Xbox 360 / XInput

本地测试包在原有 DS / Pro 模式外加入 Xbox 360 / XInput。先为 LCTech BL616 刷入 r4 固件，再启动同版配置工具，选择「Xbox 360 / XInput」，点击「保存并应用」。首次进入该身份时，浏览器可能需要重新授权独立的厂商配置接口。可通过相同接口切回 DS / Pro，无需为切模式重新刷写。

- 模式编号为 5；0–4 保持原有含义，旧配置可直接读取。
- Windows USB 身份为 045e:028e；XUSB 游戏接口与 WebHID 配置接口相互独立。
- ×→A、○→B、□→X、△→Y；Create→Back、Options→Start、PS→Guide。
- 保留模拟 L2/R2、四轴摇杆和左右震动；摇杆 128 为精确零点，覆盖完整 int16 范围，Y 轴按 XInput 方向转换。
- Xbox 模式无 USB 音频、体感、自适应扳机及键鼠接口；本功能用于电脑 XInput 游戏，不包含 Xbox 主机认证。
- USB 复位、挂起及断开会发送停止震动；蓝牙断线会提交中性输入，避免主机保留按键。
- 本轮已完成协议、描述符、网页模拟设备回归与固件构建，尚未验证 Windows 原生 XInput 驱动绑定和真实震动效果。

验收时请检查 Windows 将其识别为 Xbox 360 手柄，逐项检查按键、双摇杆中心与端点、独立扳机、左右震动和停止，再检查 Xbox → DS → Pro → Xbox 的配置回读与重连。


## alpha.6 / r5：Xbox 音频与 Pro 2 实验

先自行刷入 r5，使用同版配置工具。旧模式 0–5 保留编号，新 Pro 2 是模式 6。r4 固件不提供音频扩展和 Pro 2，不能只更新网页获得固件能力。

- Xbox：选择「Xbox 360 / XInput」，保存并应用后，Windows 会看到新的复合身份 `1209:DB05`，浏览器首次需要授权其配置接口。音频输出选择 `DS5 Link Xbox + Audio`；四通道中的 1/2 是声音，3/4 是触觉；普通声音可通过「音频转触觉」设置生成反馈。此模式无麦克风。
- Pro 2：选择「Switch 2 Pro（实验）」。该模式独立实现 HID 和 WinUSB Bulk 命令路径，不是给 Pro+麦克风改名。需要支持 Switch 2 的 SDL/libusb；真实 Windows 驱动、游戏输入与目标主机尚待验收。当前无耳麦音频，震动仅提供 SDL 单帧格式的基础转换，完整 HD Rumble 2 未实现。
- Switch 主机先尝试原有纯 Pro；Pro+麦克风继续可选，但其复合音频主机兼容性待测。Pro 2 实验代码通过软件夹具不代表 Switch 2 会接受完整初始化。
- Xbox / PlayStation 主机尚未有已验证的认证路径。请勿将电脑显示的 XInput / DualSense 名称当作主机兼容保证。

完整范围、端点分配、测试边界与验收顺序见 [平台兼容表](COMPATIBILITY_CN.md)。


## alpha.8 / r7：龙神体感与 HD 诊断

修复龙神初始化 HD 功能时发送 80 05 导致接收器退回简单报告的问题。请关闭龙神后自行刷入 r7，拔插接收器，重新在龙神控制器设置里选择当前 Pro 设备并保存，再启动游戏。体感和 HD 震动需分别勾选，游戏内体感也需开启。此修复覆盖纯 Pro / Pro+麦克风的共同协议，不把 Pro 2 的实验功能升级为完整 HD Rumble 2。

新版本地页的「连接诊断」提供原始陀螺仪、加速度、应用 IMU 开关、主机震动报告计数和蓝牙发送计数。左/右/双侧 HD 试振只在模式 3/4 和新鲜蓝牙输入下启用，600ms 自动结束，可手动停止，固件上限为 1000ms，不保存配置。离线或旧固件不会启用试振。

旧 r6 已通过 SDL 单独传感器读取和用户确认的短时 HD 效果；r7 仍需刷入后验证喷射战士内体感。详见 [平台兼容与诊断步骤](COMPATIBILITY_CN.md)。


## alpha.9 / r8：主机握手测试版

针对 r7 在 NS2 不识别的反馈，补充 Pro 启动通知和 Pro2 原生 EP0、USB 配对挑战与初始化。NS1 只以纯 Pro 为兼容目标；NS2 分别测试纯 Pro、Pro＋麦克风、Pro2。r8 尚无本接收器主机实测，不承诺三种模式已可用。Pro2 主机 0x09 体感、完整 HD Rumble 2 和耳麦音频未完成。参考项目的主机证据、修改与测试清单见包内 NS2_CONSOLE_RESEARCH_CN.md。
