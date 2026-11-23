# One-click install for the dsaudio MCP server on Windows.
#
#   irm https://github.com/thiensubs/dsaudio/releases/latest/download/install.ps1 | iex
#
# Picks the right asset, checks its SHA-256, installs to %LOCALAPPDATA%\dsaudio\bin
# and prints the MCP client snippet.
[CmdletBinding()]
param(
    [string]$Prefix = "",
    [string]$Repo = "thiensubs/dsaudio"
)

$ErrorActionPreference = "Stop"

$arch = if ([Environment]::Is64BitOperatingSystem) {
    switch ($env:PROCESSOR_ARCHITECTURE) {
        "AMD64" { "x86_64" }
        "ARM64" { "arm64" }
        default { throw "unsupported architecture: $($env:PROCESSOR_ARCHITECTURE)" }
    }
} else { throw "32-bit Windows is not supported" }

$asset = "dsaudio-mcp-windows-$arch.zip"
$url = "https://github.com/$Repo/releases/latest/download/$asset"
$destDir = if ($Prefix) { $Prefix } else { "$env:LOCALAPPDATA\dsaudio\bin" }
$tmp = Join-Path $env:TEMP "dsaudio-install"

New-Item -ItemType Directory -Force -Path $destDir, $tmp | Out-Null

Write-Host "downloading $url"
Invoke-WebRequest -Uri $url -OutFile (Join-Path $tmp $asset)

$shaUrl = "$url.sha256"
try {
    $expected = (Invoke-WebRequest -Uri $shaUrl -UseBasicParsing).Content.Split(" ")[0].Trim()
    $actual = (Get-FileHash -Algorithm SHA256 (Join-Path $tmp $asset)).Hash.ToLower()
    if ($expected -and $expected.ToLower() -ne $actual) {
        throw "checksum mismatch: expected $expected, got $actual"
    }
    Write-Host "checksum ok"
} catch {
    Write-Host "warning: no .sha256 published, skipping verification"
}

Expand-Archive -Path (Join-Path $tmp $asset) -DestinationPath $tmp -Force
$bin = Get-ChildItem -Path $tmp -Recurse -Filter "dsaudio-mcp.exe" | Select-Object -First 1
if (-not $bin) { throw "the archive did not contain dsaudio-mcp.exe" }
Copy-Item $bin.FullName (Join-Path $destDir "dsaudio-mcp.exe") -Force

if (-not ($env:PATH -split ";" -contains $destDir)) {
    Write-Host "note: $destDir is not on PATH; add it in Settings > Environment Variables"
}

Write-Host ""
Write-Host "installed $destDir\dsaudio-mcp.exe"
Write-Host ""
Write-Host "add this to your MCP client:"
Write-Host ""
Write-Host '{'
Write-Host '  "mcpServers": {'
Write-Host '    "deepseek-audio": {'
Write-Host "      `"command`": `"$destDir\dsaudio-mcp.exe`","
Write-Host '      "args": []'
Write-Host '    }'
Write-Host '  }'
Write-Host '}'
Write-Host ""
Remove-Item -Recurse -Force $tmp
