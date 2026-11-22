# 启动依赖修复

新版 GCC 16 构建导入 clock_gettime64，旧版随附 libwinpthread-1.dll 不提供该入口。
已替换为同一构建工具链 mingw64/bin/libwinpthread-1.dll。
验证：导出表包含 clock_gettime64；启动测试进程成功创建 LiteReplay 主窗口且 Responding=True。
后续重新打包必须随附匹配的线程运行库。此验证不覆盖录制和界面交互回归。
