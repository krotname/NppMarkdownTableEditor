param(
	[string]$Configuration = "Release",
	[string]$Platform = "x64",
	[string]$Version = "0.5.0"
)

$ErrorActionPreference = "Stop"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -Scope Global -ErrorAction SilentlyContinue) {
	$global:PSNativeCommandUseErrorActionPreference = $true
}

function Get-VisualStudioPath {
	$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
	if (-not (Test-Path -LiteralPath $vswhere)) {
		throw "vswhere.exe not found"
	}

	$path = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
	if (-not $path) {
		throw "Visual Studio Build Tools with MSBuild not found"
	}
	return $path
}

function Invoke-VsCommand([string]$Command) {
	$vsPath = Get-VisualStudioPath
	$vsDevCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"
	if (-not (Test-Path -LiteralPath $vsDevCmd)) {
		throw "VsDevCmd.bat not found: $vsDevCmd"
	}

	cmd.exe /d /s /c "`"$vsDevCmd`" -arch=amd64 -host_arch=amd64 >nul && $Command"
	if ($LASTEXITCODE -ne 0) {
		throw "Command failed with exit code $LASTEXITCODE"
	}
}

function Get-OutputDirectory([string]$ProjectRoot, [string]$Platform) {
	if ($Platform -eq "x64") {
		return Join-Path $ProjectRoot "bin64"
	}
	if ($Platform -eq "ARM64") {
		return Join-Path $ProjectRoot "arm64"
	}
	return Join-Path $ProjectRoot "bin"
}

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ProjectRoot "build"
$PackageRoot = Join-Path $BuildDir "dist"
$PackageDir = Join-Path $PackageRoot "MarkdownTableEditor"
$ZipPath = Join-Path $BuildDir "MarkdownTableEditor-$Version-$Platform.zip"
$PluginProject = Join-Path $ProjectRoot "vs.proj\NppPluginTemplate.vcxproj"
$TestProject = Join-Path $ProjectRoot "tests\CoreSmoke.vcxproj"

if (Test-Path -LiteralPath $BuildDir) {
	Remove-Item -LiteralPath $BuildDir -Recurse -Force
}
New-Item -ItemType Directory -Path $PackageDir | Out-Null

Invoke-VsCommand "msbuild `"$PluginProject`" /t:Rebuild /p:Configuration=$Configuration /p:Platform=$Platform /m"
Invoke-VsCommand "msbuild `"$TestProject`" /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m"

$TestExe = Join-Path $ProjectRoot "tests\CoreSmoke.exe"
if (-not (Test-Path -LiteralPath $TestExe)) {
	throw "Test executable not found: $TestExe"
}
& $TestExe
if ($LASTEXITCODE -ne 0) {
	throw "Core smoke tests failed with exit code $LASTEXITCODE"
}

$OutputDir = Get-OutputDirectory $ProjectRoot $Platform
$DllPath = Join-Path $OutputDir "MarkdownTableEditor.dll"
if (-not (Test-Path -LiteralPath $DllPath)) {
	throw "Plugin DLL not found: $DllPath"
}

Copy-Item -LiteralPath $DllPath -Destination $PackageDir -Force
Copy-Item -LiteralPath (Join-Path $ProjectRoot "license.txt") -Destination $PackageDir -Force
Copy-Item -LiteralPath (Join-Path $ProjectRoot "readme.FIRST") -Destination $PackageDir -Force

if (Test-Path -LiteralPath $ZipPath) {
	Remove-Item -LiteralPath $ZipPath -Force
}
Compress-Archive -LiteralPath $PackageDir -DestinationPath $ZipPath -CompressionLevel Optimal

Write-Output "Built $ZipPath"
