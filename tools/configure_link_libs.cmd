@echo off
REM SPDX-License-Identifier: Apache-2.0
REM Arduino IDE only (Windows): write {build.path}\link_libs.rsp for the final link.
REM Matter + chipoble_bt_matter SDK (c3/c5/h2 lib-builder) swaps -lbt for -lbt.matter.
REM PlatformIO is unchanged (pioarduino-build.py uses flags\ld_libs).

setlocal EnableExtensions EnableDelayedExpansion

if "%~2"=="" (
	echo usage: configure_link_libs.cmd ^<build.path^> ^<compiler.sdk.path^> >&2
	exit /b 2
)

set "BUILD_PATH=%~1"
set "SDK_PATH=%~2"
set "FLAGS_DIR=%SDK_PATH%\flags"
set "OUT=%BUILD_PATH%\link_libs.rsp"
set "LD_LIBS=%FLAGS_DIR%\ld_libs"

if not exist "%LD_LIBS%" (
	echo configure_link_libs: missing %LD_LIBS% >&2
	exit /b 1
)

if not exist "%BUILD_PATH%" mkdir "%BUILD_PATH%"

set "USE_MATTER_BT=1"
if not exist "%BUILD_PATH%\libraries\Matter" set "USE_MATTER_BT=0"
if not exist "%FLAGS_DIR%\chipoble_bt_matter" set "USE_MATTER_BT=0"
if not exist "%FLAGS_DIR%\ld_libs_no_bt" set "USE_MATTER_BT=0"
if not exist "%FLAGS_DIR%\matter_bt_lib" set "USE_MATTER_BT=0"

if "%USE_MATTER_BT%"=="1" (
	if not exist "%SDK_PATH%\lib\libbt.matter.a" (
		echo configure_link_libs: Matter BLE optimization requires %SDK_PATH%\lib\libbt.matter.a >&2
		echo configure_link_libs: use esp32-arduino-libs for esp32c3/c5/h2 with bt_matter_harvest, or update the core SDK. >&2
		exit /b 1
	)
	set "LD_LIBS_NO_BT=%FLAGS_DIR%\ld_libs_no_bt"
	set "MATTER_BT_LIB=%FLAGS_DIR%\matter_bt_lib"
	REM set /p truncates at 1023 chars; ld_libs is several KB — use PowerShell like configure_link_libs.sh.
	powershell -NoProfile -ExecutionPolicy Bypass -Command ^
		"$n=[IO.File]::ReadAllText('!LD_LIBS_NO_BT!').Trim();" ^
		"$m=[IO.File]::ReadAllText('!MATTER_BT_LIB!').Trim();" ^
		"[IO.File]::WriteAllText('!OUT!', ($n + ' ' + $m))"
	if errorlevel 1 (
		echo configure_link_libs: failed to write %OUT% >&2
		exit /b 1
	)
) else (
	copy /y "%LD_LIBS%" "%OUT%" >nul
)

exit /b 0
