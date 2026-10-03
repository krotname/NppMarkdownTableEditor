$ErrorActionPreference = "Stop"

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$smokeScript = Join-Path $PSScriptRoot "Invoke-NppCompatibilitySmoke.ps1"

function Assert-WorkDirRejected([string]$Path, [string]$Scenario, [string]$ExpectedMessage = "WorkDir must be a child directory under the project root") {
    $previousErrorPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $smokeScript -WorkDir $Path 2>&1
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorPreference
    }
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
    $previousErrorPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $smokeScript `
            -WorkDir $Path `
            -Win32PluginPath $missingPlugin `
            -X64PluginPath $missingPlugin 2>&1
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorPreference
    }
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

$tokens = $null
$parseErrors = $null
$smokeAst = [System.Management.Automation.Language.Parser]::ParseFile($smokeScript, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) {
    throw "Compatibility smoke script contains parse errors."
}
$resolver = $smokeAst.Find({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq "Resolve-LatestNotepadPortable"
}, $true)
Invoke-Expression $resolver.Extent.Text

function Invoke-RestMethod([hashtable]$Headers, [string]$Uri, [int]$TimeoutSec) {
    if ($TimeoutSec -ne 30) { throw "Latest release lookup has no bounded timeout." }
    $script:lastReleaseHeaders = $Headers
    return [pscustomobject]@{
        tag_name = "test"
        name = "test"
        html_url = $Uri
        assets = @([pscustomobject]@{ name = "npp.test.portable.x64.zip"; browser_download_url = "https://example.invalid/npp.zip" })
    }
}

$previousToken = $env:GH_TOKEN
try {
    $env:GH_TOKEN = "smoke-test-token"
    $NotepadLatestPortableUrl = ""
    foreach ($NotepadLatestApiUrl in @(
        "https://api.github.com/repos/notepad-plus-plus/notepad-plus-plus/releases/latest",
        "http://api.github.com/releases/latest",
        "https://api.github.com.example.invalid/releases/latest",
        "https://example.invalid/releases/latest"
    )) {
        $null = Resolve-LatestNotepadPortable
        $expectedAuth = $NotepadLatestApiUrl.StartsWith("https://api.github.com/")
        if ($script:lastReleaseHeaders.ContainsKey("Authorization") -ne $expectedAuth) {
            throw "Latest release lookup authenticated an unexpected API target."
        }
        if ($expectedAuth -and $script:lastReleaseHeaders["Authorization"] -ne "Bearer smoke-test-token") {
            throw "Latest release lookup did not use the supplied token."
        }
    }
    $env:GH_TOKEN = ""
    $NotepadLatestApiUrl = "https://api.github.com/repos/notepad-plus-plus/notepad-plus-plus/releases/latest"
    $null = Resolve-LatestNotepadPortable
    if ($script:lastReleaseHeaders.ContainsKey("Authorization")) {
        throw "Latest release lookup added authentication without a token."
    }
}
finally {
    $env:GH_TOKEN = $previousToken
}

Write-Host "Notepad++ compatibility smoke path and API-authentication safety tests passed"
