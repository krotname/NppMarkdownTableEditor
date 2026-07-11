$ErrorActionPreference = "Stop"

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$smokeScript = Join-Path $PSScriptRoot "Invoke-NppCompatibilitySmoke.ps1"

function Assert-WorkDirRejected([string]$Path, [string]$Scenario, [string]$ExpectedMessage = "WorkDir must be a child directory under the project root") {
    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $smokeScript -WorkDir $Path 2>&1
    $exitCode = $LASTEXITCODE
    $text = $output | Out-String
    if ($exitCode -eq 0) {
        throw "$Scenario unexpectedly accepted unsafe WorkDir '$Path'."
    }
    if ($text -notmatch [regex]::Escape($ExpectedMessage)) {
        throw "$Scenario failed for an unexpected reason:`n$text"
    }
}

function Assert-SafeWorkDirReachesPluginValidation([string]$Path) {
    $missingPlugin = Join-Path $projectRoot "build\path-safety-tests\missing-plugin.dll"
    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $smokeScript `
        -WorkDir $Path `
        -Win32PluginPath $missingPlugin `
        -X64PluginPath $missingPlugin 2>&1
    $exitCode = $LASTEXITCODE
    $text = $output | Out-String
    if ($exitCode -eq 0 -or $text -notmatch "Win32 plugin DLL not found") {
        throw "Safe child WorkDir did not reach plugin validation as expected:`n$text"
    }
}

Assert-WorkDirRejected -Path $projectRoot -Scenario "Project-root boundary"
Assert-WorkDirRejected -Path ($projectRoot + "-sibling") -Scenario "Sibling-prefix boundary"

$probeDir = Join-Path $projectRoot "build\path-safety-tests"
$probeFile = Join-Path $probeDir "not-a-directory"
New-Item -ItemType Directory -Path $probeDir -Force | Out-Null
Set-Content -LiteralPath $probeFile -Value "sentinel" -Encoding Ascii -NoNewline
try {
    Assert-SafeWorkDirReachesPluginValidation -Path (Join-Path $probeDir "safe-child")
    Assert-WorkDirRejected -Path $probeFile -Scenario "File WorkDir boundary" -ExpectedMessage "WorkDir must be a directory, not a file"
    if ((Get-Content -LiteralPath $probeFile -Raw) -ne "sentinel") {
        throw "File WorkDir boundary modified its sentinel file."
    }
}
finally {
    $resolvedProbeDir = [IO.Path]::GetFullPath($probeDir)
    $expectedProbeDir = [IO.Path]::GetFullPath((Join-Path $projectRoot "build\path-safety-tests"))
    if (-not $resolvedProbeDir.Equals($expectedProbeDir, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove unexpected safety-test directory: $resolvedProbeDir"
    }
    Remove-Item -LiteralPath $resolvedProbeDir -Recurse -Force
}

Write-Host "Notepad++ compatibility smoke path-safety tests passed"
