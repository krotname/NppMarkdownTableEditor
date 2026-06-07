param(
	[string]$Configuration = "Debug",
	[string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -Scope Global -ErrorAction SilentlyContinue) {
	$global:PSNativeCommandUseErrorActionPreference = $true
}

function Write-Utf8File([string]$Path, [string[]]$Lines) {
	$encoding = New-Object Text.UTF8Encoding($false)
	[IO.File]::WriteAllLines($Path, $Lines, $encoding)
}

function Format-Decimal([double]$Value) {
	return $Value.ToString("0.###############", [Globalization.CultureInfo]::InvariantCulture)
}

function Format-Rate([int]$Covered, [int]$Valid) {
	if ($Valid -eq 0) {
		return "n/a"
	}
	return "{0:P2}" -f ($Covered / $Valid)
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

function Get-ClassLineStats($ClassNode) {
	$map = @{}
	foreach ($line in @($ClassNode.SelectNodes(".//line"))) {
		$number = [int]$line.GetAttribute("number")
		$hits = [int]$line.GetAttribute("hits")
		if (-not $map.ContainsKey($number) -or $hits -gt $map[$number]) {
			$map[$number] = $hits
		}
	}

	$covered = 0
	foreach ($hits in $map.Values) {
		if ($hits -gt 0) {
			$covered++
		}
	}

	return [pscustomobject]@{
		Covered = $covered
		Valid = $map.Count
	}
}

function Get-ReportLineRows($Document) {
	$map = @{}
	foreach ($class in @($Document.SelectNodes("//class"))) {
		$file = $class.GetAttribute("filename")
		foreach ($line in @($class.SelectNodes(".//line"))) {
			$number = [int]$line.GetAttribute("number")
			$hits = [int]$line.GetAttribute("hits")
			$key = "$file`:$number"
			if (-not $map.ContainsKey($key) -or $hits -gt $map[$key].Hits) {
				$map[$key] = [pscustomobject]@{
					File = $file
					Number = $number
					Hits = $hits
				}
			}
		}
	}
	return @($map.Values)
}

function Save-XmlNoBom($Document, [string]$Path) {
	$settings = New-Object Xml.XmlWriterSettings
	$settings.Encoding = New-Object Text.UTF8Encoding($false)
	$settings.Indent = $true
	$writer = [Xml.XmlWriter]::Create($Path, $settings)
	try {
		$Document.Save($writer)
	} finally {
		$writer.Close()
	}
}

function HtmlEncode([string]$Value) {
	return [Net.WebUtility]::HtmlEncode($Value)
}

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ProjectRoot "build"
$CoverageWorkDir = Join-Path $BuildDir "coverage"
$ReportsDir = Join-Path $BuildDir "reports\coverage"
$HtmlDir = Join-Path $ReportsDir "html"
$RawCoberturaPath = Join-Path $CoverageWorkDir "coverage.raw.cobertura.xml"
$CoberturaPath = Join-Path $ReportsDir "coverage.cobertura.xml"
$SummaryJsonPath = Join-Path $ReportsDir "summary.json"
$SummaryMarkdownPath = Join-Path $ReportsDir "coverage-summary.md"
$HtmlPath = Join-Path $HtmlDir "index.html"

if (Test-Path -LiteralPath $CoverageWorkDir) {
	Remove-Item -LiteralPath $CoverageWorkDir -Recurse -Force
}
if (Test-Path -LiteralPath $ReportsDir) {
	Remove-Item -LiteralPath $ReportsDir -Recurse -Force
}
New-Item -ItemType Directory -Path $CoverageWorkDir, $HtmlDir | Out-Null

$VsPath = Get-VisualStudioPath
$VsDevCmd = Join-Path $VsPath "Common7\Tools\VsDevCmd.bat"
$CoverageConsole = Join-Path $VsPath "Common7\IDE\Extensions\Microsoft\CodeCoverage.Console\Microsoft.CodeCoverage.Console.exe"
if (-not (Test-Path -LiteralPath $VsDevCmd)) {
	throw "VsDevCmd.bat not found: $VsDevCmd"
}
if (-not (Test-Path -LiteralPath $CoverageConsole)) {
	throw "Microsoft.CodeCoverage.Console.exe not found: $CoverageConsole"
}

$ProjectFile = Join-Path $ProjectRoot "tests\CoreSmoke.vcxproj"
$BuildCommand = "`"$VsDevCmd`" -arch=amd64 -host_arch=amd64 >nul && msbuild `"$ProjectFile`" /t:Rebuild /p:Configuration=$Configuration /p:Platform=$Platform /m"
cmd.exe /d /s /c $BuildCommand
if ($LASTEXITCODE -ne 0) {
	throw "MSBuild failed with exit code $LASTEXITCODE"
}

$TestExe = Join-Path $ProjectRoot "tests\CoreSmoke.exe"
if (-not (Test-Path -LiteralPath $TestExe)) {
	throw "Test executable not found: $TestExe"
}

$CoreSource = Join-Path $ProjectRoot "src\MarkdownTableCore.cpp"
$TestSourcePattern = Join-Path $ProjectRoot "tests\*.cpp"

& $CoverageConsole collect --nologo `
	--output $RawCoberturaPath `
	--output-format cobertura `
	--include-files $CoreSource `
	--include-files $TestSourcePattern `
	$TestExe
if ($LASTEXITCODE -ne 0) {
	throw "Coverage collection failed with exit code $LASTEXITCODE"
}

[xml]$CoverageXml = Get-Content -LiteralPath $RawCoberturaPath
$SourceRoot = (Resolve-Path (Join-Path $ProjectRoot "src")).Path

foreach ($class in @($CoverageXml.SelectNodes("//class"))) {
	$filename = $class.GetAttribute("filename")
	if (-not $filename.StartsWith($SourceRoot, [StringComparison]::OrdinalIgnoreCase)) {
		[void]$class.ParentNode.RemoveChild($class)
	}
}

foreach ($package in @($CoverageXml.SelectNodes("//package"))) {
	if ($package.SelectNodes("classes/class").Count -eq 0) {
		[void]$package.ParentNode.RemoveChild($package)
	}
}

foreach ($class in @($CoverageXml.SelectNodes("//class"))) {
	$stats = Get-ClassLineStats $class
	$rate = if ($stats.Valid -eq 0) { 1.0 } else { $stats.Covered / $stats.Valid }
	$class.SetAttribute("line-rate", (Format-Decimal $rate))
}

foreach ($package in @($CoverageXml.SelectNodes("//package"))) {
	$rows = @()
	foreach ($class in @($package.SelectNodes("classes/class"))) {
		$rows += Get-ReportLineRows ([xml]$class.OuterXml)
	}
	$valid = $rows.Count
	$covered = @($rows | Where-Object { $_.Hits -gt 0 }).Count
	$rate = if ($valid -eq 0) { 1.0 } else { $covered / $valid }
	$package.SetAttribute("line-rate", (Format-Decimal $rate))
}

$LineRows = Get-ReportLineRows $CoverageXml
$LinesValid = $LineRows.Count
$LinesCovered = @($LineRows | Where-Object { $_.Hits -gt 0 }).Count
$LineRate = if ($LinesValid -eq 0) { 1.0 } else { $LinesCovered / $LinesValid }
$CoverageXml.coverage.SetAttribute("line-rate", (Format-Decimal $LineRate))
$CoverageXml.coverage.SetAttribute("lines-covered", [string]$LinesCovered)
$CoverageXml.coverage.SetAttribute("lines-valid", [string]$LinesValid)
Save-XmlNoBom $CoverageXml $CoberturaPath

$FileStats = @(
	$LineRows |
		Group-Object File |
		Sort-Object Name |
		ForEach-Object {
			$valid = $_.Group.Count
			$covered = @($_.Group | Where-Object { $_.Hits -gt 0 }).Count
			$uncovered = @($_.Group | Where-Object { $_.Hits -eq 0 } | Sort-Object Number | ForEach-Object { $_.Number })
			[pscustomobject]@{
				File = $_.Name
				RelativeFile = [IO.Path]::GetRelativePath($ProjectRoot, $_.Name)
				LinesCovered = $covered
				LinesValid = $valid
				LineCoverage = Format-Rate $covered $valid
				UncoveredLines = $uncovered
			}
		}
)

$Summary = [ordered]@{
	project = "NppMarkdownTableEditor"
	lineCoverage = Format-Rate $LinesCovered $LinesValid
	linesCovered = $LinesCovered
	linesValid = $LinesValid
	rawCoberturaReport = $RawCoberturaPath
	coberturaReport = $CoberturaPath
	htmlReport = $HtmlPath
	files = $FileStats
}
$Summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $SummaryJsonPath -Encoding UTF8

$Markdown = @(
	"# C++ coverage report"
	""
	"Generated: $(Get-Date -Format s)"
	""
	"| File | Covered lines | Valid lines | Coverage |"
	"| --- | ---: | ---: | ---: |"
)
foreach ($file in $FileStats) {
	$Markdown += "| $($file.RelativeFile) | $($file.LinesCovered) | $($file.LinesValid) | $($file.LineCoverage) |"
}
$Markdown += ""
$Markdown += "Total line coverage: $(Format-Rate $LinesCovered $LinesValid) ($LinesCovered / $LinesValid)"
$Markdown += "Cobertura XML: $CoberturaPath"
$Markdown += "HTML: $HtmlPath"
Write-Utf8File $SummaryMarkdownPath $Markdown

$RowsHtml = @()
foreach ($file in $FileStats) {
	$uncoveredText = if ($file.UncoveredLines.Count -eq 0) { "" } else { ($file.UncoveredLines -join ", ") }
	$RowsHtml += "<tr><td>$(HtmlEncode $file.RelativeFile)</td><td>$($file.LinesCovered)</td><td>$($file.LinesValid)</td><td>$($file.LineCoverage)</td><td>$(HtmlEncode $uncoveredText)</td></tr>"
}

$Html = @(
	"<!doctype html>"
	"<html lang=""en"">"
	"<head>"
	"<meta charset=""utf-8"">"
	"<title>C++ Coverage Report</title>"
	"<style>"
	"body{font-family:Segoe UI,Arial,sans-serif;margin:32px;color:#1f2937;background:#fff}"
	"h1{font-size:28px;margin:0 0 8px}"
	".summary{font-size:18px;margin:0 0 24px}"
	"table{border-collapse:collapse;width:100%;max-width:1100px}"
	"th,td{border:1px solid #d1d5db;padding:8px 10px;text-align:left;vertical-align:top}"
	"th{background:#f3f4f6}"
	"td:nth-child(2),td:nth-child(3),td:nth-child(4){text-align:right;white-space:nowrap}"
	"</style>"
	"</head>"
	"<body>"
	"<h1>C++ Coverage Report</h1>"
	"<p class=""summary"">Total line coverage: <strong>$(Format-Rate $LinesCovered $LinesValid)</strong> ($LinesCovered / $LinesValid)</p>"
	"<table>"
	"<thead><tr><th>File</th><th>Covered lines</th><th>Valid lines</th><th>Coverage</th><th>Uncovered lines</th></tr></thead>"
	"<tbody>"
) + $RowsHtml + @(
	"</tbody>"
	"</table>"
	"</body>"
	"</html>"
)
Write-Utf8File $HtmlPath $Html

Write-Output "Coverage summary: $SummaryMarkdownPath"
Write-Output "HTML report: $HtmlPath"
Write-Output "Cobertura report: $CoberturaPath"
