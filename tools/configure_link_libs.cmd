@echo off
REM SPDX-License-Identifier: Apache-2.0
REM Arduino IDE only (Windows): write {build.path}\link_libs.rsp for the final link.
REM Matter sketch + lib\libbt.matter.a: replace -lbt with -lbt.matter in flags\ld_libs.

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

set "SWAP_MATTER_BT=0"
if exist "%BUILD_PATH%\libraries\Matter" if exist "%SDK_PATH%\lib\libbt.matter.a" set "SWAP_MATTER_BT=1"

if "!SWAP_MATTER_BT!"=="1" (
	powershell -NoProfile -ExecutionPolicy Bypass -Command ^
		"$t=[IO.File]::ReadAllText('!LD_LIBS!');" ^
		"$t=[regex]::Replace($t,'(^| )-lbt( |$)','${1}-lbt.matter${2}');" ^
		"[IO.File]::WriteAllText('!OUT!', $t)"
	if errorlevel 1 (
		echo configure_link_libs: failed to write %OUT% >&2
		exit /b 1
	)
) else (
	copy /y "%LD_LIBS%" "%OUT%" >nul
)

exit /b 0
