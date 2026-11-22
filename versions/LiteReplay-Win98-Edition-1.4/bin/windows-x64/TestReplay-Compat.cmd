@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo Test 720p30 capture and system audio. CPU encoding may increase CPU usage.
LiteReplayProbe.exe --capture --allow-cpu > compat-probe-log.txt 2>&1
set "probe_exit=%errorlevel%"
type compat-probe-log.txt
pause
exit /b %probe_exit%
