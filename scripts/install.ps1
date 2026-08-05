#Requires -Version 5.1
<#
.SYNOPSIS
  Install the latest WebSearchFree (wsf) Windows release binary.

.EXAMPLE
  irm https://raw.githubusercontent.com/drmikecrypto/WebSearchFree/main/scripts/install.ps1 | iex

.EXAMPLE
  .\scripts\install.ps1 -InstallDir "$env:LOCALAPPDATA\WebSearchFree"
#>
[CmdletBinding()]
param(
  [string]$Repo = $(if ($env:WSF_REPO) { $env:WSF_REPO } else { "drmikecrypto/WebSearchFree" }),
  [string]$InstallDir = $(if ($env:WSF_INSTALL_DIR) { $env:WSF_INSTALL_DIR } else { Join-Path $env:LOCALAPPDATA "WebSearchFree" }),
  [string]$Asset = "wsf-windows-amd64.exe"
)

$ErrorActionPreference = "Stop"

Write-Host "Fetching latest release from $Repo…"
$release = Invoke-RestMethod -Uri "https://api.github.com/repos/$Repo/releases/latest" -Headers @{
  "User-Agent" = "WebSearchFree-install"
  "Accept"     = "application/vnd.github+json"
}

$assetObj = $release.assets | Where-Object { $_.name -eq $Asset } | Select-Object -First 1
if (-not $assetObj) {
  throw "Could not find asset '$Asset' in the latest GitHub release. Tag a v* release first, or use Docker."
}

New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
$dest = Join-Path $InstallDir "wsf.exe"

Write-Host "Downloading $($assetObj.browser_download_url)"
Invoke-WebRequest -Uri $assetObj.browser_download_url -OutFile $dest -UseBasicParsing

Write-Host ""
Write-Host "Installed: $dest"
Write-Host ""
Write-Host "Add to PATH for this session:"
Write-Host "  `$env:PATH = '$InstallDir;' + `$env:PATH"
Write-Host ""
Write-Host "Try:"
Write-Host "  & '$dest' search `"open source metasearch`" --max 3"
Write-Host "  & '$dest' serve --port 8080"
Write-Host "  # open http://127.0.0.1:8080"

# Persist user PATH if not present
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($userPath -notlike "*$InstallDir*") {
  [Environment]::SetEnvironmentVariable("Path", ($InstallDir.TrimEnd('\') + ";" + $userPath), "User")
  Write-Host ""
  Write-Host "Added $InstallDir to your user PATH (new terminals will see 'wsf')."
}

$env:PATH = "$InstallDir;" + $env:PATH
