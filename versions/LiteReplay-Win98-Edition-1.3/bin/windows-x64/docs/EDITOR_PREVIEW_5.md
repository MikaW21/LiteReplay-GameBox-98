# LiteReplay 2.0 Editor Preview 5

软件由 Yuzi_Lj 制作。

## 桌面性能覆盖层

1. 打开主界面的「性能监视」页，点击「显示 / 隐藏覆盖层」，或按 Ctrl+Shift+F7。程序缩到托盘时也可以切换。Ctrl+Shift+F9 仍控制原有回放 HUD。
2. 从项目列表选择一项，勾选显示，并用「上移」「下移」调整顺序。字号、透明度和列数会即时应用。
3. 取消「锁定并鼠标穿透」后，按住覆盖层拖动到任意显示器的可用区域。位置与显示项目一起保存到当前预设。可在三套预设间切换，也可重置位置。
4. 进程下拉框可以回到系统总量，也可以选择特定 PID。覆盖层里的「所选进程」显示该进程的 CPU、GPU、RAM；其他硬件项目继续显示整机数据。

## 读数与数据来源

- CPU 提供总使用率、各逻辑核心使用率、可读的温度和频率。逻辑核心编号采用 Windows 性能计数器的处理器组与核心编号。
- GPU 使用率采用最忙引擎。专用显存用量汇总自 GPU 性能计数器，总容量取自 DXGI。集成显卡可能没有可报告的专用显存总量。
- 温度显示可读传感器中的最高值，CPU、GPU 时钟显示可读的最高当前核心频率。支持的 NVIDIA 驱动提供 GPU 温度与频率。CPU 温度及其他 GPU 的传感器可通过具有 WMI 输出的 [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor) 提供。启动该工具后可用 `Get-CimInstance -Namespace root\LibreHardwareMonitor -ClassName Sensor` 检查当前账户能否读取。缺少传感器或计数器时显示「不可用」。
- RAM 显示整机已用、总量和百分比。进程 RAM 为工作集。磁盘速率汇总物理磁盘读写，网络速率汇总接口收发。
- 选中游戏的渲染进程后，勾选「采集此应用的 FPS」。内置的 [Intel PresentMon 2.3.1](https://github.com/GameTechDev/PresentMon/releases/tag/v2.3.1) 从 Windows ETW 采集应用提交帧。在最近约五秒内，取提交最频繁的交换链计算 FPS 和平均帧时间。累计至少 100 帧时计算最慢 1% 帧的平均帧时间对应的 FPS。
- FPS 采集需要 Windows ETW 权限。遇到「Windows 拒绝 ETW 跟踪」时，可使用已有跟踪权限的账户，例如 Performance Log Users 成员，也可在需要采集时以管理员身份运行 LiteReplay。桌面覆盖层适用于桌面和可合成窗口，如无边框模式。独占全屏下可能被应用遮挡。

## 进程调度

选择运行中的进程、设定优先级和允许使用的逻辑核心后，点击「应用调度设置」。调度设置对当前运行实例生效，「恢复修改前设置」恢复本次 LiteReplay 会话第一次调整前的值。进程必须允许当前账号修改。实时优先级仅显示已有状态。存在多个 Windows 处理器组时，该版仍可查看全部核心用量，进程面板只提供优先级修改。

CPU 占用率作为读数实时刷新。优先级与核心亲和性由 Windows 调度器执行。
