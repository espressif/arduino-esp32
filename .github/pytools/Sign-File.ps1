[CmdletBinding()]
param (
    [Parameter()]
    [String]
    $Path
)


function FindSignTool {
    $SignTool = "signtool.exe"
    if (Get-Command $SignTool -ErrorAction SilentlyContinue) {
        return $SignTool
    }
    $SignTool = "${env:ProgramFiles(x86)}\Windows Kits\10\bin\x64\signtool.exe"
    if (Test-Path -Path $SignTool -PathType Leaf) {
        return $SignTool
    }
    $SignTool = "${env:ProgramFiles(x86)}\Windows Kits\10\bin\x86\signtool.exe"
    if (Test-Path -Path $SignTool -PathType Leaf) {
        return $SignTool
    }
    $sdkVers = "10.0.22000.0", "10.0.20348.0", "10.0.19041.0", "10.0.17763.0"
    Foreach ($ver in $sdkVers)
    {
        $SignTool = "${env:ProgramFiles(x86)}\Windows Kits\10\bin\${ver}\x64\signtool.exe"
        if (Test-Path -Path $SignTool -PathType Leaf) {
            return $SignTool
        }
    }
    "signtool.exe not found"
    Exit 1
}

function SignEsptool {
    param(
        [Parameter()]
        [String]
        $Path
    )

    $SignTool = FindSignTool
    "Using: $SignTool"
    $CertificateFile = [system.io.path]::GetTempPath() + "certificate.pfx"

    if ($null -eq $env:CERTIFICATE) {
        "CERTIFICATE variable not set, unable to sign the file"
        Exit 1
    }

    if ("" -eq $env:CERTIFICATE) {
        "CERTIFICATE variable is empty, unable to sign the file"
        Exit 1
    }

    $SignParameters = @("sign", "/tr", 'http://timestamp.digicert.com', "/td", "SHA256", "/f", $CertificateFile, "/fd", "SHA256")
    if ($env:CERTIFICATE_PASSWORD) {
        "CERTIFICATE_PASSWORD detected, using the password"
        $SignParameters += "/p"
        $SignParameters += $env:CERTIFICATE_PASSWORD
    }
    $SignParameters += $Path

    [byte[]]$CertificateBytes = [convert]::FromBase64String($env:CERTIFICATE)
    [IO.File]::WriteAllBytes($CertificateFile, $CertificateBytes)

    &$SignTool $SignParameters

    if (0 -eq $LASTEXITCODE) {
        Remove-Item $CertificateFile
    } else {
        Remove-Item $CertificateFile
        "Signing failed"
        Exit 1
    }

}

SignEsptool ${Path}
