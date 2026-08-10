# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
#
# Run inside a Windows Docker container: install Arduino IDE 1.x, ESP32 core, compile test sketch.
#
# Usage: windows_ide_v1_test.ps1 -ArduinoIdeVersion <ver> -PackageJsonPath <path> -SketchPath <relative> [-ReleaseVersion <ver>]

param(
    [Parameter(Mandatory = $true)]
    [string]$ArduinoIdeVersion,

    [Parameter(Mandatory = $true)]
    [string]$PackageJsonPath,

    [Parameter(Mandatory = $true)]
    [string]$SketchPath,

    [string]$ReleaseVersion = ""
)

$ErrorActionPreference = "Stop"

Write-Host "=== Downloading Arduino IDE $ArduinoIdeVersion ==="
$url = "https://downloads.arduino.cc/arduino-$ArduinoIdeVersion-windows.zip"
Invoke-WebRequest -Uri $url -OutFile C:\arduino.zip -UseBasicParsing
Expand-Archive -Path C:\arduino.zip -DestinationPath C:\
Remove-Item C:\arduino.zip

$arduinoCmd = "C:\arduino-$ArduinoIdeVersion\arduino_debug.exe"

Write-Host "=== Installing ESP32 core via IDE v1 ==="
$packageUrl = "file:///$($PackageJsonPath -replace '\\', '/')"
$versionSuffix = if ($ReleaseVersion) { ":$ReleaseVersion" } else { "" }
& $arduinoCmd --pref "boardsmanager.additional.urls=$packageUrl" --install-boards "esp32:esp32$versionSuffix"
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "=== Compiling test sketch ==="
$sketchWinPath = Join-Path "C:\workspace" ($SketchPath -replace '/', '\')
& $arduinoCmd --pref "boardsmanager.additional.urls=$packageUrl" --verify --board esp32:esp32:esp32 $sketchWinPath
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "=== SUCCESS ==="
