@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo LiteReplay 诊断：将采集约 5 秒桌面画面和默认设备的系统声音。
echo 视频输出：LiteReplayTest 文件夹。日志：probe-log.txt
LiteReplayProbe.exe --capture > probe-log.txt 2>&1
set "probe_exit=%errorlevel%"
type probe-log.txt
echo.
echo 退出代码：%probe_exit%
pause
exit /b %probe_exit%
