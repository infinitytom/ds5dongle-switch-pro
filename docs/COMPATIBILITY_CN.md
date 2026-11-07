# 平台兼容与当前边界（alpha.8 / r7）

此表描述本项目接收器，不等于原装手柄的兼容表。识别名称、软件协议测试、真实游戏输入与长时间认证是不同验收阶段。

| 平台 | 当前可选方案 | 已有证据与尚需验证 |
|---|---|---|
| Windows 电脑 / 模拟器 | DS / Edge、Xbox / XInput、Pro；Pro 2 实验 | r3 的 Pro 握手、SDL3 实际打开和独立麦克风采集通过。r5 的 Xbox 音频与 Pro 2 仅有软件验证，原生驱动绑定和游戏流程待测 |
| Linux / macOS / SteamOS | 优先尝试 DS 或 Pro；XInput 和 Pro 2 取决于驱动 | 当前没有本项目实机验收；Pro 2 要求支持该协议的 SDL / libusb 构建 |
| Android / USB OTG 设备 | 优先尝试 DS / Pro 标准输入 | 系统和游戏决定支持情况；未实机验收 |
| Switch 一代 | Pro，模式 3 | USB 有线通信开启后测试；主机尚未验收。模式 4 的独立麦克风不能由电脑测试推断为主机可用 |
| Switch 2 | 优先测试已有 Pro，另提供 Pro 2 实验模式 6 | 主机均未验收。Pro 2 是基于公开 SDL/VIIPER 路径的协议实现，不代表复现了 Switch 2 的完整初始化、认证或耳麦功能 |
| Xbox 360 / One / Series | 当前没有已验证的主机专用模式 | 模式 5 是电脑 XInput，不能作为 Xbox 主机支持。后续需要对应主机协议及认证路径；Series/One 也不能用 360 名称代替其协议 |
| PS5 | DS / Edge 保持现有电脑协议实现 | USB 识别、主机认证、PS5 原生游戏及长期不断连均未验收，不承诺可用 |
| PS3 / PS4 | 当前没有已验证的专用模式 | 需要独立模式与对应主机验收。DualSense 模式不能直接视为 DualShock 4；原装 DualSense 本身也不支持 PS4 主机 |
| 更早的非 USB 主机 | 需要额外硬件或外部转换器 | 当前接收器的输出是 USB，不能只靠固件选择项提供其他物理接口 |

## 模式 5：电脑 XInput + 音频触觉

- 使用复合设备测试身份 `1209:DB05`，XUSB 游戏接口与配置 HID、UAC1 音频分开；此标识参考 DS5_Bridge 的复合设备方案，不是本项目获得了厂商 VID 分配或主机授权。
- 仍接受旧 r4 身份 `045E:028E` 的网页配置连接；旧固件不会因此自动获得音频。
- UAC1 输出为 4 通道、16bit、48kHz：前两通道声音，后两通道 DualSense 触觉波形。系统或应用需实际输出到 `DS5 Link Xbox + Audio`。
- 「音频转触觉」关闭时读取后两通道；自动在后两通道静音时使用前两通道；强制使用前两通道生成触觉。音频生成反馈不能恢复普通 XInput 命令中不存在的游戏原始 HD 细节。
- 音频流打开时发送经典震动停止/释放命令，音频拥有触觉输出；缓存其间的最新 XInput 震动，关闭音频流时恢复。普通 XInput 输出使用 DualSense 改进型震动，零马达命令释放音频通道。
- 本板只有 4 个 FIFO：音频 OUT 1、配置 IN 2、XInput OUT 3、游戏 IN 4。同编号 IN/OUT 不混用。本模式没有 USB 麦克风；DS 模式和 Pro+麦克风模式继续保留各自输入功能。

## 模式 6：Switch 2 Pro 实验

- USB 身份 `057E:2069`。HID 接口 0 提供输入 `05/09`、输出 `02` 和独立厂商配置 TLC；接口 1 是 WinUSB Bulk 命令通道。
- 4 个 FIFO：HID OUT 1、Bulk IN 2、Bulk OUT 3、HID IN 4。Full-Speed Bulk MPS 64；High-Speed/其他速度描述符正确使用对应的 512/64，不共用双向 FIFO。
- 支持 SDL 风格的只读校准块、功能位、输入报告格式选择和开始/停止报告命令；80 字节校准响应在固件中分成 64+16 发送。未实现的命令拒绝，避免假装已完成认证或音频初始化。
- `05` 提供按钮、双摇杆、按真实 DualSense 校准转换的三轴加速度/陀螺仪和时间戳；`09` 提供标准 HID 按钮/摇杆布局。静音键在 `05` 中映射 C；普通 DS 没有独立 GL/GR，不伪造背键输入。
- 仅对 SDL 使用的 `0x5` 单帧震动格式做左右独立的基础强度转换。频率和其他打包格式没有转译成完整 HD Rumble 2 波形；这不是 HD Rumble 2 完整支持。
- 无耳麦音频、NFC、蓝牙直连目标主机、主机唤醒或已验证的 Switch 2 主机认证。本模式没有替代一代 Pro+麦克风。

## 验收顺序

每个平台分别记录：USB 枚举 → 驱动打开 → 全按键/摇杆/扳机 → 体感/震动/音频 → 断线重连与睡眠恢复 → 持续游戏。每一项都需真实硬件证据，不能由描述符测试推断。

优先用一台电脑验证 r5 的三种新增链路：Xbox 游戏接口和 UAC1 同时打开、Xbox 音频/经典震动切换、Pro 2 的 SDL 实际打开。再测试 Switch 1/2 主机；涉及 PlayStation / Xbox 的认证方案另行实现和验收。

参考：[SDL Switch2 驱动](https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_switch2.c)、[VIIPER ns2pro](https://github.com/Alia5/VIIPER/tree/main/device/ns2pro)、[GP2040-CE 主机兼容说明](https://gp2040-ce.info/faq/faq-console-compatibility/)、[Sony 的 DualSense/PS4 兼容说明](https://blog.playstation.com/2020/11/09/ps5-the-ultimate-faq/)。这些参考项目的结果不是本接收器的实机验收。


## r6：浏览器键位修正及用户反馈

用户已报告各模式可在电脑网站识别；Pro 2 在 https://www.gamepadtester.cn/ 上键位错乱。此反馈只证明被识别，不代表全部输入或主机通过。

r5 默认 0x09 错用了一代 Pro 的报告位序，现已单独编码；浏览器 HID 描述符将 Nintendo 位序映射到标准按钮 Usage，同时将四轴放在连续的 Usage 0x30–0x33，便于该网站按 axes[0..3] 读取。0x09 是通用 HID 回退，Y 轴向下为正；SDL 的 0x05 报告继续使用 Nintendo Y 轴向上为正。此调整没有实现新的主机认证，也没有修复未实现的 Pro 2 耳麦音频或完整 HD Rumble 2。

软件逐键测试和描述符解析已通过，真实浏览器仍需用刷入 r6 的接收器复测。为避免读取旧 HID 描述符，Pro 2 的 bcdDevice 已从 0200 提升到 0201；刷后拔插接收器、刷新网页，再分别测试面键、L/R、ZL/ZR、菜单键、四向与双摇杆。

主机测试建议先在 Switch 1/2 选择纯 Pro 并开启有线通信；分别记录是否识别、能否控制、体感与震动、持续游玩和重连。Pro 2 继续列为主机待测，Xbox / PlayStation 认证状态不变。


## r7：龙神初始化与实际链路验证

2026-11-07 使用用户龙神目录附带的 SDL3 3.5.0，连接现有 r6 纯 Pro：SDL 加速度计和陀螺仪均可启用并读到非零、持续更新的数据；通过 SDL_SendGamepadEffect 发送低强度左右 HD 包后，用户确认有短暂震感。这证明该模式的传感器和触觉链路可工作，不代表 Pro+麦克风、Pro 2 或主机一起通过。

进一步按龙神 NpadHdRumble 的初始化顺序发送 80 05，在 r6 实机观察到报告从 0x30 退回 0x3F，重新开启传感器也未恢复。r7 修正该命令语义，保留完整报告、IMU 和振动状态；回归测试覆盖这一顺序。原生手柄的 80 04/05 是 USB 超时相关控制，不是选择输入报告格式。

r7 的 FA 诊断和 F6/10 本地试振独立于主机协商状态，不保存配置。纯 Pro / Pro+麦克风支持低强度 320/160Hz、左右独立、最长 1000ms 的本地试振；Pro 2 仅提供数据诊断，不能以这个入口声称支持完整 HD Rumble 2。

用户当前测试游戏为《喷射战士3》，并确认游戏内体感已开启。配置检查发现 HD 震动关闭，已备份后仅将 use_hdrumble 设为 true；游戏绑定没有被自动替换。另一个绑定旧 DualSense 的游戏是《任天堂明星大乱斗》，不能作为喷射战士问题的根因。

更新步骤：关闭龙神 → 刷入 r7 → 拔插接收器并选择纯 Pro → 在龙神重新选择当前 Nintendo Switch Pro Controller、启用体感和 HD 震动 → 保存后启动喷射战士复测。版本更新或切换 USB 身份可能改变 SDL 设备 GUID，旧下拉选项不能代表仍绑定了当前设备。

连接诊断页可在游戏运行时查看 IMU 开关和报告模式：应是已启用与 0x30；若变成 0x3F，完整体感报告尚未生效。源陀螺仪变化而应用未启用 IMU，应检查当前 SDL 设备。HD 本地试振有感觉而主机震动报告数不增加，应检查模拟器的游戏震动输出，而不是把它当作执行器故障。

复用探测：`py -3 tools/probe_switch_sdl.py --sdl <龙神目录>/SDL3.dll`；关闭模拟器后运行避免两个 SDL 客户端争用同一设备，`--rumble` 可显式加入短时 HD 测试并发送停止命令。脚本不写接收器配置或刷固件。

参考：[龙神 NpadHdRumble.InitializeDevice](https://github.com/NextendoNetwork/Ryujinx-Nextendo/blob/main/src/Ryujinx.Input.SDL3/NpadHdRumble.cs)。r7 尚待刷入后的真实游戏验收；本轮未验证主机兼容。
