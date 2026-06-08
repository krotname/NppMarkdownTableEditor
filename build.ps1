param(
	[string]$Configuration = "Release",
	[string]$Platform = "x64",
	[string]$Version = ""
)

$ErrorActionPreference = "Stop"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -Scope Global -ErrorAction SilentlyContinue) {
	$global:PSNativeCommandUseErrorActionPreference = $true
}

function Get-VisualStudioPath {
	$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
	$path = $null
	if (Test-Path -LiteralPath $vswhere) {
		$path = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
	}

	if (-not $path) {
		$fallback = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2022\BuildTools"
		if (Test-Path -LiteralPath (Join-Path $fallback "Common7\Tools\VsDevCmd.bat")) {
			return $fallback
		}
		throw "Visual Studio Build Tools with MSBuild not found"
	}
	return ($path | Select-Object -First 1)
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

function Read-ProjectVersion([string]$ProjectRoot) {
	$versionPath = Join-Path $ProjectRoot "VERSION"
	if (-not (Test-Path -LiteralPath $versionPath)) {
		throw "VERSION file not found: $versionPath"
	}
	return (Get-Content -LiteralPath $versionPath -Raw).Trim()
}

function Convert-ToVersionParts([string]$Version) {
	if ($Version -notmatch '^(\d+)\.(\d+)\.(\d+)$') {
		throw "Version must use major.minor.patch numeric format, for example 0.6.0. Got: $Version"
	}

	return [pscustomobject]@{
		Major = [int]$matches[1]
		Minor = [int]$matches[2]
		Patch = [int]$matches[3]
		Build = 0
	}
}

function Assert-DllVersion([string]$DllPath, [string]$Version) {
	$versionInfo = (Get-Item -LiteralPath $DllPath).VersionInfo
	if ($versionInfo.FileVersion -ne $Version) {
		throw "DLL FileVersion mismatch. Expected $Version, got $($versionInfo.FileVersion)"
	}
	if ($versionInfo.ProductVersion -ne $Version) {
		throw "DLL ProductVersion mismatch. Expected $Version, got $($versionInfo.ProductVersion)"
	}
}

function Assert-ZipContains([string]$ZipPath, [string[]]$Entries) {
	Add-Type -AssemblyName System.IO.Compression.FileSystem
	$zip = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
	try {
		$actualEntries = @($zip.Entries | ForEach-Object { $_.FullName })

		foreach ($entry in $Entries) {
			if ($actualEntries -notcontains $entry) {
				throw "ZIP package is missing required license/readme entry '$entry': $ZipPath"
			}
		}
	}
	finally {
		$zip.Dispose()
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

function Resolve-BuildPlatform([string]$Platform) {
	if ($Platform -eq "x86") {
		return "Win32"
	}
	if ($Platform -eq "arm" -or $Platform -eq "arm64") {
		return "ARM64"
	}
	return $Platform
}

function Get-PackagePlatformLabel([string]$Platform) {
	if ($Platform -eq "Win32") {
		return "x86"
	}
	if ($Platform -eq "ARM64") {
		return "arm64"
	}
	return $Platform
}

$BuildPlatform = Resolve-BuildPlatform $Platform
$PackagePlatform = Get-PackagePlatformLabel $BuildPlatform
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Version = if ([string]::IsNullOrWhiteSpace($Version)) { Read-ProjectVersion $ProjectRoot } else { $Version.Trim() }
$VersionParts = Convert-ToVersionParts $Version

$BuildDir = Join-Path $ProjectRoot "build"
$PackageRoot = Join-Path $BuildDir "dist"
$PackageDir = Join-Path $PackageRoot "MarkdownTableEditor"
$ZipPath = Join-Path $BuildDir "MarkdownTableEditor-$Version-$PackagePlatform.zip"
$PluginAdminZipPath = Join-Path $BuildDir "MarkdownTableEditor-$Version-$PackagePlatform-pluginadmin.zip"
$PluginProject = Join-Path $ProjectRoot "vs.proj\MarkdownTableEditor.vcxproj"
$TestProject = Join-Path $ProjectRoot "tests\CoreSmoke.vcxproj"

if (Test-Path -LiteralPath $BuildDir) {
	Remove-Item -LiteralPath $BuildDir -Recurse -Force
}
New-Item -ItemType Directory -Path $PackageDir | Out-Null

Invoke-VsCommand "msbuild `"$PluginProject`" /t:Rebuild /p:Configuration=$Configuration /p:Platform=$BuildPlatform /p:MarkdownTableEditorVersionMajor=$($VersionParts.Major) /p:MarkdownTableEditorVersionMinor=$($VersionParts.Minor) /p:MarkdownTableEditorVersionPatch=$($VersionParts.Patch) /p:MarkdownTableEditorVersionBuild=$($VersionParts.Build) /m"
Invoke-VsCommand "msbuild `"$TestProject`" /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m"

$TestExe = Join-Path $ProjectRoot "tests\CoreSmoke.exe"
if (-not (Test-Path -LiteralPath $TestExe)) {
	throw "Test executable not found: $TestExe"
}
& $TestExe
if ($LASTEXITCODE -ne 0) {
	throw "Core smoke tests failed with exit code $LASTEXITCODE"
}

$OutputDir = Get-OutputDirectory $ProjectRoot $BuildPlatform
$DllPath = Join-Path $OutputDir "MarkdownTableEditor.dll"
if (-not (Test-Path -LiteralPath $DllPath)) {
	throw "Plugin DLL not found: $DllPath"
}
Assert-DllVersion $DllPath $Version

Copy-Item -LiteralPath $DllPath -Destination $PackageDir -Force
Copy-Item -LiteralPath (Join-Path $ProjectRoot "license.txt") -Destination $PackageDir -Force
Copy-Item -LiteralPath (Join-Path $ProjectRoot "readme.FIRST") -Destination $PackageDir -Force
Copy-Item -LiteralPath (Join-Path $ProjectRoot "NOTICE.md") -Destination $PackageDir -Force
Copy-Item -LiteralPath (Join-Path $ProjectRoot "LICENSES") -Destination $PackageDir -Recurse -Force

if (Test-Path -LiteralPath $ZipPath) {
	Remove-Item -LiteralPath $ZipPath -Force
}
Compress-Archive -LiteralPath $PackageDir -DestinationPath $ZipPath -CompressionLevel Optimal

if (Test-Path -LiteralPath $PluginAdminZipPath) {
	Remove-Item -LiteralPath $PluginAdminZipPath -Force
}
Compress-Archive -LiteralPath @(
	(Join-Path $PackageDir "MarkdownTableEditor.dll"),
	(Join-Path $PackageDir "license.txt"),
	(Join-Path $PackageDir "readme.FIRST"),
	(Join-Path $PackageDir "NOTICE.md"),
	(Join-Path $PackageDir "LICENSES")
) -DestinationPath $PluginAdminZipPath -CompressionLevel Optimal

Assert-ZipContains $PluginAdminZipPath @(
	"MarkdownTableEditor.dll",
	"license.txt",
	"readme.FIRST",
	"NOTICE.md",
	"LICENSES/GPL-3.0-or-later.txt",
	"LICENSES/LGPL-3.0-or-later.txt",
	"LICENSES/Scintilla.txt"
)

Write-Output "Built $ZipPath"
Write-Output "Built $PluginAdminZipPath"
