## Win98 Edition 1.6

- 新增游戏会话历史、手动识别和素材归档。
- 新增游戏状态通知、可关闭的合成提示音及磁盘空间面板。

# 2026-11-17 · Retro Edition 1.0

新增应用内经典桌面、开始菜单、任务栏、托盘状态和多窗口管理。迁移回放、媒体、剪辑、性能、音频和设置页面，新增关于、使用统计和 DOS 彩蛋。详见 docs/RETRO_EDITION.md。

# 2026-11-17 · Editor Preview 7

性能覆盖层支持单独拖动指标卡片，位置随预设保存。经典 98 加入渐变标题条和状态栏，新增磷光终端与霓虹街机主题。详见 docs/EDITOR_PREVIEW_7.md。

# 2026-11-17 · Editor Preview 6

设置新增界面主题，支持即时切换及保存。提供石墨深色、经典 98、雾白浅色和碳黑。主题应用于主界面、剪辑、音频和桌面性能覆盖层。详见 docs/EDITOR_PREVIEW_6.md。

# 2026-11-17 · Editor Preview 5

新增可定制桌面性能覆盖层、所选进程的 FPS 采集与 CPU 调度设置。更新主界面导航、首页和性能页。详见 docs/EDITOR_PREVIEW_5.md。

# 2026-11-13 · UI Preview 1

软件由 Yuzi_Lj 制作 / Software by Yuzi_Lj

原生深色导航、页面标题、状态与负载首页、设置卡片分组、剪辑区布局和空预览提示。保留 HF2 预览逻辑。Windows UI/DPI 实机待测。详见 docs/UI_PREVIEW_1.md。

# 2026-11-10 · Phase3a Hotfix 2

软件由 Yuzi_Lj 制作 / Software by Yuzi_Lj

用户确认 HF1 仍在 SetStartStopPosition 失败。本版移除此调用，完整文件预览使用绝对 seek，100ms 活跃定时器检测片段出点（非逐帧精确）；导出不变。窗口标识 HF2。Windows 实机待复测。

# 2026-11-10 · Phase3a Hotfix 1

软件由 Yuzi_Lj 制作 / Software by Yuzi_Lj

修正 MFPlay 预览范围、相对 seek 和播放头坐标；新增异步加载保护、具体错误日志和范围回归测试。用户视频及 Windows 实机验证待完成。

# 2.0 Phase3a — 2026-11-09

软件由 Yuzi_Lj 制作 / Software by Yuzi_Lj

- 将原 LiteCut 核心复用并嵌入同一 LiteReplay 的视频剪辑页。
- 实现导入、所选片段预览、裁剪、分割、排序、复制/删除及片段音量。
- 时间轴支持定位、缩放/横向滚动、视频和关联音频状态显示。
- 增加有界撤销/重做、工程读写、自动保存与恢复。
- MP4 重编码导出提供 NVENC H.264/HEVC 和明确标注的 CPU MPEG-4；选中片段可 stream copy 快速裁剪，提示关键帧限制。
- 媒体库进入剪辑，导出成功自动入库；回放启动会取消编辑任务，回放运行时禁止预览/导出。
- P3a 实机验收待完成；BGM、字幕、变速、代理和完整多轨仍待后续实现。

# 2.0 Phase2c — 2026-11-09

软件由 Yuzi_Lj 制作 / Software by Yuzi_Lj

- 整屏和区域截图同时复制图片到剪贴板，支持 Ctrl+V 粘贴。
- 剪贴板占用时用短定时器有限重试；复制和 PNG 保存分别报告结果。
- PNG 截图保存成功后自动入库；支持 PNG 导入、筛选和文件管理。
- PNG 由默认图片查看器打开，不将视频播放器错误用于图片。
- 退出时等待 PNG 写入并登记保存结果。
- 用户反馈 Phase2b 截图功能可用；本版新增功能仅完成构建/逻辑测试，尚无实机结果。

# 2.0 Phase2b — 2026-11-09

软件由 Yuzi_Lj 制作 / Software by Yuzi_Lj

- 新增快速整屏 PNG 截图和拖选区域 PNG 截图。
- 设置页提供两组可保存的快捷键，检查重复和已有热键占用。
- 区域截图支持 Esc / 右键 / 焦点切换取消；保存不覆盖已有同名文件。
- 媒体库新增受管理的按需缩略图提取、超时取消、有界 LRU 缓存和失败提示。
- 首页、设置、主窗口标题及后续发布流程加入 Yuzi_Lj 署名。
- 保留 AMD / Intel / CPU 兼容路径。未作 Windows 实机、性能或长期稳定性验证。

## 历史记录

# 2.0 compatibility preview — 2026-11-09

- Enumerate attached outputs across AMD, Intel and NVIDIA adapters.
- Add vendor-aware NVENC/AMF choice and Windows MF H.264 hardware/software paths.
- Keep CPU fallback opt-in; reject unsupported MF codec combinations without silently changing format.
- Add bounded CPU NV12 frame transfer pool for MF and report the actual selected backend.
- Publish replay codec parameters only after stream headers are available.
- Add a 720p30 old-laptop preset and capture diagnostics with display/backend selection.
- Rebuild bundled FFmpeg with AMF and Media Foundation encoders and preserve matching source/notices.
- Add encoder policy regression tests. Real hardware, synchronization and performance acceptance remain pending.
