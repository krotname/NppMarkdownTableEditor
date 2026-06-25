param(
    [string]$WorkDir = "",
    [string]$Win32PluginPath = "",
    [string]$X64PluginPath = "",
    [string]$Notepad759Url = "https://github.com/notepad-plus-plus/notepad-plus-plus/releases/download/v7.5.9/npp.7.5.9.bin.zip",
    [string]$Notepad831Url = "https://github.com/notepad-plus-plus/notepad-plus-plus/releases/download/v8.3.1/npp.8.3.1.portable.x64.zip",
    [string]$NotepadLatestApiUrl = "https://api.github.com/repos/notepad-plus-plus/notepad-plus-plus/releases/latest",
    [string]$NotepadLatestPortableUrl = "",
    [switch]$IncludeLatest,
    [switch]$LatestOnly
)

$ErrorActionPreference = "Stop"

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if (-not $WorkDir) {
    $WorkDir = Join-Path $projectRoot ($(if ($LatestOnly) { "build\npp-latest" } else { "build\npp-compat" }))
} elseif (-not [IO.Path]::IsPathRooted($WorkDir)) {
    $WorkDir = Join-Path $projectRoot $WorkDir
}
$WorkDir = [IO.Path]::GetFullPath($WorkDir)

if (-not $WorkDir.StartsWith($projectRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "WorkDir must stay under the project root. Got: $WorkDir"
}

if (-not $Win32PluginPath) {
    $Win32PluginPath = Join-Path $projectRoot "bin\MarkdownTableEditor.dll"
}
if (-not $X64PluginPath) {
    $X64PluginPath = Join-Path $projectRoot "bin64\MarkdownTableEditor.dll"
}

$Win32PluginPath = [IO.Path]::GetFullPath($Win32PluginPath)
$X64PluginPath = [IO.Path]::GetFullPath($X64PluginPath)
if (-not $LatestOnly -and -not (Test-Path -LiteralPath $Win32PluginPath)) {
    throw "Win32 plugin DLL not found: $Win32PluginPath"
}
if (-not (Test-Path -LiteralPath $X64PluginPath)) {
    throw "x64 plugin DLL not found: $X64PluginPath"
}

Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class MarkdownTableEditorNppSmokeNative {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern IntPtr GetMenu(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern int GetMenuItemCount(IntPtr hMenu);

    [DllImport("user32.dll")]
    public static extern IntPtr GetSubMenu(IntPtr hMenu, int nPos);

    [DllImport("user32.dll")]
    public static extern int GetMenuItemID(IntPtr hMenu, int nPos);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetMenuString(IntPtr hMenu, int uIDItem, StringBuilder lpString, int nMaxCount, uint uFlag);

    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);
}
'@

$MF_BYPOSITION = 0x400
$WM_COMMAND = 0x0111
$WM_CLOSE = 0x0010

function Reset-Directory([string]$Path) {
    $resolved = [IO.Path]::GetFullPath($Path)
    if (-not $resolved.StartsWith($WorkDir, [StringComparison]::OrdinalIgnoreCase) -and $resolved -ne $WorkDir) {
        throw "Refusing to remove path outside WorkDir: $resolved"
    }
    if (Test-Path -LiteralPath $resolved) {
        $removeArgs = @{ Recurse = $true; Force = $true }
        Remove-Item -LiteralPath $resolved @removeArgs
    }
    New-Item -ItemType Directory -Path $resolved | Out-Null
}

function Download-And-Extract([string]$Name, [string]$Url, [string]$ZipName) {
    $zipPath = Join-Path $WorkDir $ZipName
    $targetDir = Join-Path $WorkDir $Name

    Write-Host "Downloading $ZipName"
    Invoke-WebRequest -Uri $Url -OutFile $zipPath

    Reset-Directory $targetDir
    Expand-Archive -LiteralPath $zipPath -DestinationPath $targetDir -Force
    $exe = Get-ChildItem -Path $targetDir -Filter "notepad++.exe" -Recurse | Select-Object -First 1
    if (-not $exe) {
        throw "notepad++.exe not found after extracting $ZipName"
    }
    return $exe.FullName
}

function Resolve-LatestNotepadPortable {
    if ($NotepadLatestPortableUrl) {
        $uri = [Uri]$NotepadLatestPortableUrl
        $assetName = [IO.Path]::GetFileName($uri.AbsolutePath)
        if (-not $assetName) {
            $assetName = "npp.latest.portable.x64.zip"
        }
        return [pscustomobject]@{
            Tag = "custom"
            Name = "custom"
            AssetName = $assetName
            Url = $NotepadLatestPortableUrl
            HtmlUrl = $NotepadLatestPortableUrl
        }
    }

    Write-Host "Resolving latest Notepad++ release"
    $release = Invoke-RestMethod -Headers @{ "User-Agent" = "MarkdownTableEditor-Smoke" } -Uri $NotepadLatestApiUrl
    $asset = @($release.assets | Where-Object {
        $_.name -match "portable\.x64\.zip$" -and $_.name -notmatch "sha256"
    } | Select-Object -First 1)
    if ($asset.Count -eq 0) {
        throw "Latest Notepad++ release does not expose a portable x64 ZIP asset: $($release.html_url)"
    }

    return [pscustomobject]@{
        Tag = [string]$release.tag_name
        Name = [string]$release.name
        AssetName = [string]$asset[0].name
        Url = [string]$asset[0].browser_download_url
        HtmlUrl = [string]$release.html_url
    }
}

function Install-X64Plugin([string]$ExePath, [string]$PluginPath) {
    $pluginsDir = Join-Path (Split-Path $ExePath -Parent) "plugins\MarkdownTableEditor"
    New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
    Copy-Item -LiteralPath $PluginPath -Destination (Join-Path $pluginsDir "MarkdownTableEditor.dll") -Force
}

function Get-RawMenuText([IntPtr]$Menu, [int]$Position) {
    $buffer = [Text.StringBuilder]::new(512)
    [void][MarkdownTableEditorNppSmokeNative]::GetMenuString($Menu, $Position, $buffer, $buffer.Capacity, $MF_BYPOSITION)
    return ($buffer.ToString() -replace "&", "").Trim()
}

function Get-MenuText([IntPtr]$Menu, [int]$Position) {
    return ((Get-RawMenuText $Menu $Position) -replace "\t.*$", "").Trim()
}

function Find-MenuItemId([IntPtr]$Menu, [string]$Needle, [switch]$Exact) {
    if ($Menu -eq [IntPtr]::Zero) {
        return $null
    }

    $count = [MarkdownTableEditorNppSmokeNative]::GetMenuItemCount($Menu)
    for ($i = 0; $i -lt $count; $i++) {
        $rawText = Get-RawMenuText $Menu $i
        $text = Get-MenuText $Menu $i
        $id = [MarkdownTableEditorNppSmokeNative]::GetMenuItemID($Menu, $i)
        $matches = if ($Exact) { $text -eq $Needle } else { $text -like "*$Needle*" -or $rawText -like "*$Needle*" }
        if ($id -ne -1 -and $matches) {
            return $id
        }

        $subMenu = [MarkdownTableEditorNppSmokeNative]::GetSubMenu($Menu, $i)
        if ($subMenu -ne [IntPtr]::Zero) {
            $found = Find-MenuItemId $subMenu $Needle -Exact:$Exact
            if ($null -ne $found) {
                return $found
            }
        }
    }
    return $null
}

function Get-ProcessDialogs([int]$ProcessId) {
    $script:DialogProcessId = $ProcessId
    $script:DialogWindows = New-Object System.Collections.Generic.List[object]
    $callback = [MarkdownTableEditorNppSmokeNative+EnumWindowsProc]{
        param([IntPtr]$hWnd, [IntPtr]$lParam)

        [uint32]$windowProcessId = 0
        [void][MarkdownTableEditorNppSmokeNative]::GetWindowThreadProcessId($hWnd, [ref]$windowProcessId)
        if ($windowProcessId -eq [uint32]$script:DialogProcessId -and [MarkdownTableEditorNppSmokeNative]::IsWindowVisible($hWnd)) {
            $className = [Text.StringBuilder]::new(128)
            $title = [Text.StringBuilder]::new(512)
            [void][MarkdownTableEditorNppSmokeNative]::GetClassName($hWnd, $className, $className.Capacity)
            [void][MarkdownTableEditorNppSmokeNative]::GetWindowText($hWnd, $title, $title.Capacity)
            if ($className.ToString() -eq "#32770") {
                $script:DialogWindows.Add([pscustomobject]@{
                    Handle = $hWnd
                    Title = $title.ToString()
                }) | Out-Null
            }
        }
        return $true
    }

    [void][MarkdownTableEditorNppSmokeNative]::EnumWindows($callback, [IntPtr]::Zero)
    return $script:DialogWindows
}

function Invoke-ConvertSmoke([string]$Name, [string]$ExePath) {
    $inputText = "Name,Score`r`nAnna,10`r`n"
    $expected = "| Name | Score |`n| ---- | ----- |`n| Anna | 10    |"
    $testFile = Join-Path (Split-Path $ExePath -Parent) "mte-smoke.csv"
    Set-Content -LiteralPath $testFile -Value $inputText -Encoding UTF8 -NoNewline

    $process = Start-Process -FilePath $ExePath -ArgumentList @("-multiInst", "-nosession", $testFile) -WindowStyle Minimized -PassThru
    try {
        $mainWindow = [IntPtr]::Zero
        for ($i = 0; $i -lt 80; $i++) {
            Start-Sleep -Milliseconds 250
            $process.Refresh()
            if ($process.MainWindowHandle -ne 0) {
                $mainWindow = [IntPtr]$process.MainWindowHandle
                break
            }
        }
        if ($mainWindow -eq [IntPtr]::Zero) {
            throw "${Name}: Notepad++ main window was not found"
        }

        $convertId = $null
        for ($i = 0; $i -lt 40; $i++) {
            Start-Sleep -Milliseconds 250
            $convertId = Find-MenuItemId ([MarkdownTableEditorNppSmokeNative]::GetMenu($mainWindow)) "CSV/TSV"
            if ($null -ne $convertId) {
                break
            }
        }
        if ($null -eq $convertId) {
            throw "${Name}: plugin convert menu item was not found"
        }

        [void][MarkdownTableEditorNppSmokeNative]::PostMessage($mainWindow, $WM_COMMAND, [IntPtr]$convertId, [IntPtr]::Zero)
        Start-Sleep -Seconds 1

        $dialogs = @(Get-ProcessDialogs $process.Id)
        foreach ($dialog in $dialogs) {
            [void][MarkdownTableEditorNppSmokeNative]::PostMessage($dialog.Handle, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
        }
        if ($dialogs.Count -gt 0) {
            $titles = ($dialogs | ForEach-Object { $_.Title }) -join "; "
            throw "${Name}: unexpected dialog after convert command: $titles"
        }

        $saveId = Find-MenuItemId ([MarkdownTableEditorNppSmokeNative]::GetMenu($mainWindow)) "Save" -Exact
        if ($null -eq $saveId) {
            throw "${Name}: save menu item was not found"
        }
        [void][MarkdownTableEditorNppSmokeNative]::PostMessage($mainWindow, $WM_COMMAND, [IntPtr]$saveId, [IntPtr]::Zero)
        Start-Sleep -Seconds 1

        $actual = (Get-Content -LiteralPath $testFile -Raw -Encoding UTF8) -replace "`r`n", "`n"
        $actual = $actual.TrimEnd("`r", "`n")
        if ($actual -ne $expected) {
            throw "${Name}: CSV conversion result mismatch.`nExpected:`n$expected`nActual:`n$actual"
        }

        Write-Host "${Name}: CSV/TSV menu smoke passed"
    }
    finally {
        if (-not $process.HasExited) {
            [void][MarkdownTableEditorNppSmokeNative]::PostMessage([IntPtr]$process.MainWindowHandle, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
            if (-not $process.WaitForExit(3000)) {
                $process.Kill()
                $process.WaitForExit()
            }
        }
    }
}

function Invoke-StartupAutoFormatSmoke([string]$Name, [string]$ExePath) {
    $inputText = "# title`r`n`r`n| Name | Age |`r`n| --- | ---: |`r`n| Anna | 20 |`r`n| Alexander | 7 |`r`n"
    $expectedLine = "| Name      | Age |"
    $testFile = Join-Path (Split-Path $ExePath -Parent) "mte-startup-auto.md"
    Set-Content -LiteralPath $testFile -Value $inputText -Encoding UTF8 -NoNewline

    $process = Start-Process -FilePath $ExePath -ArgumentList @("-multiInst", "-nosession", $testFile) -WindowStyle Normal -PassThru
    try {
        $mainWindow = [IntPtr]::Zero
        for ($i = 0; $i -lt 80; $i++) {
            Start-Sleep -Milliseconds 250
            $process.Refresh()
            if ($process.MainWindowHandle -ne 0) {
                $mainWindow = [IntPtr]$process.MainWindowHandle
                break
            }
        }
        if ($mainWindow -eq [IntPtr]::Zero) {
            throw "${Name}: Notepad++ main window was not found"
        }

        Start-Sleep -Seconds 3

        $dialogs = @(Get-ProcessDialogs $process.Id)
        foreach ($dialog in $dialogs) {
            [void][MarkdownTableEditorNppSmokeNative]::PostMessage($dialog.Handle, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
        }
        if ($dialogs.Count -gt 0) {
            $titles = ($dialogs | ForEach-Object { $_.Title }) -join "; "
            throw "${Name}: unexpected dialog after startup auto format: $titles"
        }

        $saveId = Find-MenuItemId ([MarkdownTableEditorNppSmokeNative]::GetMenu($mainWindow)) "Save" -Exact
        if ($null -eq $saveId) {
            throw "${Name}: save menu item was not found"
        }
        [void][MarkdownTableEditorNppSmokeNative]::PostMessage($mainWindow, $WM_COMMAND, [IntPtr]$saveId, [IntPtr]::Zero)
        Start-Sleep -Seconds 1

        $actual = (Get-Content -LiteralPath $testFile -Raw -Encoding UTF8) -replace "`r`n", "`n"
        if ($actual -notlike "*$expectedLine*") {
            throw "${Name}: startup auto format did not align the current file.`nExpected line:`n$expectedLine`nActual:`n$actual"
        }

        Write-Host "${Name}: startup auto format smoke passed"
    }
    finally {
        if (-not $process.HasExited) {
            [void][MarkdownTableEditorNppSmokeNative]::PostMessage([IntPtr]$process.MainWindowHandle, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
            if (-not $process.WaitForExit(3000)) {
                $process.Kill()
                $process.WaitForExit()
            }
        }
    }
}

function Invoke-TableMenuCommandSmoke(
    [string]$Name,
    [string]$ExePath,
    [string]$FileName,
    [string]$InputText,
    [string]$MenuNeedle,
    [string]$ExpectedText
) {
    $testFile = Join-Path (Split-Path $ExePath -Parent) $FileName
    Set-Content -LiteralPath $testFile -Value $InputText -Encoding UTF8 -NoNewline

    $process = Start-Process -FilePath $ExePath -ArgumentList @("-multiInst", "-nosession", $testFile) -WindowStyle Normal -PassThru
    try {
        $mainWindow = [IntPtr]::Zero
        for ($i = 0; $i -lt 80; $i++) {
            Start-Sleep -Milliseconds 250
            $process.Refresh()
            if ($process.MainWindowHandle -ne 0) {
                $mainWindow = [IntPtr]$process.MainWindowHandle
                break
            }
        }
        if ($mainWindow -eq [IntPtr]::Zero) {
            throw "${Name}: Notepad++ main window was not found"
        }

        $commandId = $null
        for ($i = 0; $i -lt 40; $i++) {
            Start-Sleep -Milliseconds 250
            $commandId = Find-MenuItemId ([MarkdownTableEditorNppSmokeNative]::GetMenu($mainWindow)) $MenuNeedle
            if ($null -ne $commandId) {
                break
            }
        }
        if ($null -eq $commandId) {
            throw "${Name}: plugin menu item was not found: $MenuNeedle"
        }

        [void][MarkdownTableEditorNppSmokeNative]::PostMessage($mainWindow, $WM_COMMAND, [IntPtr]$commandId, [IntPtr]::Zero)
        Start-Sleep -Seconds 1

        $dialogs = @(Get-ProcessDialogs $process.Id)
        foreach ($dialog in $dialogs) {
            [void][MarkdownTableEditorNppSmokeNative]::PostMessage($dialog.Handle, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
        }
        if ($dialogs.Count -gt 0) {
            $titles = ($dialogs | ForEach-Object { $_.Title }) -join "; "
            throw "${Name}: unexpected dialog after $MenuNeedle command: $titles"
        }

        $saveId = Find-MenuItemId ([MarkdownTableEditorNppSmokeNative]::GetMenu($mainWindow)) "Save" -Exact
        if ($null -eq $saveId) {
            throw "${Name}: save menu item was not found"
        }
        [void][MarkdownTableEditorNppSmokeNative]::PostMessage($mainWindow, $WM_COMMAND, [IntPtr]$saveId, [IntPtr]::Zero)
        Start-Sleep -Seconds 1

        $actual = (Get-Content -LiteralPath $testFile -Raw -Encoding UTF8) -replace "`r`n", "`n"
        $actual = $actual.TrimEnd("`r", "`n")
        $expected = ($ExpectedText -replace "`r`n", "`n").TrimEnd("`r", "`n")
        if ($actual -ne $expected) {
            throw "${Name}: $MenuNeedle result mismatch.`nExpected:`n$expected`nActual:`n$actual"
        }

        Write-Host "${Name}: $MenuNeedle menu smoke passed"
    }
    finally {
        if (-not $process.HasExited) {
            [void][MarkdownTableEditorNppSmokeNative]::PostMessage([IntPtr]$process.MainWindowHandle, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
            if (-not $process.WaitForExit(3000)) {
                $process.Kill()
                $process.WaitForExit()
            }
        }
    }
}

Reset-Directory $WorkDir

if (-not $LatestOnly) {
    $npp759 = Download-And-Extract "npp-7.5.9-x86" $Notepad759Url "npp.7.5.9.bin.zip"
    $npp831 = Download-And-Extract "npp-8.3.1-x64" $Notepad831Url "npp.8.3.1.portable.x64.zip"

    $plugins759 = Join-Path (Split-Path $npp759 -Parent) "plugins"
    New-Item -ItemType Directory -Path $plugins759 -Force | Out-Null
    Copy-Item -LiteralPath $Win32PluginPath -Destination (Join-Path $plugins759 "MarkdownTableEditor.dll") -Force
    Install-X64Plugin $npp831 $X64PluginPath

    Invoke-ConvertSmoke "Notepad++ 7.5.9 x86" $npp759
    Invoke-StartupAutoFormatSmoke "Notepad++ 7.5.9 x86" $npp759
    Invoke-ConvertSmoke "Notepad++ 8.3.1 x64" $npp831
    Invoke-StartupAutoFormatSmoke "Notepad++ 8.3.1 x64" $npp831
}

if ($IncludeLatest -or $LatestOnly) {
    $latest = Resolve-LatestNotepadPortable
    $safeLatestName = ($latest.Tag -replace "[^0-9A-Za-z_.-]", "-").Trim("-")
    if (-not $safeLatestName) {
        $safeLatestName = "latest"
    }
    $latestExe = Download-And-Extract "npp-$safeLatestName-x64" $latest.Url $latest.AssetName
    Install-X64Plugin $latestExe $X64PluginPath

    $latestLabel = "Notepad++ $($latest.Tag) x64"
    Write-Host "$latestLabel release page: $($latest.HtmlUrl)"
    Invoke-ConvertSmoke $latestLabel $latestExe
    Invoke-StartupAutoFormatSmoke $latestLabel $latestExe
    Invoke-TableMenuCommandSmoke `
        -Name $latestLabel `
        -ExePath $latestExe `
        -FileName "mte-sort-ascending.md" `
        -InputText "| Name | Score |`r`n| ---- | ----- |`r`n| Bob | 2 |`r`n| Anna | 10 |`r`n" `
        -MenuNeedle "Ctrl+Alt+Shift+=" `
        -ExpectedText "| Name | Score |`n| ---- | ----- |`n| Anna | 10    |`n| Bob  | 2     |"
    Invoke-TableMenuCommandSmoke `
        -Name $latestLabel `
        -ExePath $latestExe `
        -FileName "mte-sort-descending.md" `
        -InputText "| Name | Score |`r`n| ---- | ----- |`r`n| Anna | 10 |`r`n| Bob | 2 |`r`n" `
        -MenuNeedle "Ctrl+Alt+Shift+-" `
        -ExpectedText "| Name | Score |`n| ---- | ----- |`n| Bob  | 2     |`n| Anna | 10    |"
    Write-Host "$latestLabel latest regression smoke passed"
}
