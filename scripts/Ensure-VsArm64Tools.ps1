$ErrorActionPreference = "Stop"

$installerDir = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer"
$vswhere = Join-Path $installerDir "vswhere.exe"
$installer = Join-Path $installerDir "vs_installer.exe"

function Test-Arm64Compiler {
    param(
        [Parameter(Mandatory = $true)]
        [string] $InstallPath
    )

    $compilerPattern = Join-Path $InstallPath "VC\Tools\MSVC\*\bin\Host*\arm64\cl.exe"
    return [bool](Get-ChildItem -Path $compilerPattern -ErrorAction SilentlyContinue | Select-Object -First 1)
}

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe was not found: $vswhere"
}

if (-not (Test-Path -LiteralPath $installer)) {
    throw "vs_installer.exe was not found: $installer"
}

$installPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath |
    Select-Object -First 1
if (-not $installPath) {
    throw "Visual Studio installation with MSBuild was not found."
}

if (Test-Arm64Compiler -InstallPath $installPath) {
    Write-Host "ARM64 C++ build tools are already installed."
    exit 0
}

Write-Host "Installing ARM64 C++ build tools into: $installPath"
& $installer modify `
    --installPath $installPath `
    --add Microsoft.VisualStudio.Component.VC.Tools.ARM64 `
    --quiet `
    --norestart `
    --nocache

if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 3010) {
    throw "Visual Studio Installer failed with exit code $LASTEXITCODE"
}

if (-not (Test-Arm64Compiler -InstallPath $installPath)) {
    throw "ARM64 C++ compiler was not found after installing Microsoft.VisualStudio.Component.VC.Tools.ARM64."
}

Write-Host "ARM64 C++ build tools are ready."
