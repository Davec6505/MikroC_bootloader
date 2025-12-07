# MikroC USB HID Bootloader - Windows Uninstaller
# Removes mikro_hb.exe from Program Files and system PATH

param(
    [switch]$Unattended = $false
)

$ErrorActionPreference = "Stop"

# Installation configuration
$InstallName = "MikroHB"
$InstallPath = "$env:ProgramFiles\$InstallName"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " MikroC USB HID Bootloader - Uninstaller" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "ERROR: This uninstaller requires Administrator privileges." -ForegroundColor Red
    Write-Host "Please right-click and select 'Run as Administrator'" -ForegroundColor Yellow
    Write-Host ""
    pause
    exit 1
}

# Check if installed
if (-not (Test-Path $InstallPath)) {
    Write-Host "INFO: $InstallName is not installed." -ForegroundColor Yellow
    Write-Host ""
    if (-not $Unattended) {
        pause
    }
    exit 0
}

Write-Host "Found installation at:" -ForegroundColor White
Write-Host "  $InstallPath" -ForegroundColor Cyan
Write-Host ""

# Confirm uninstall
if (-not $Unattended) {
    $response = Read-Host "Do you want to uninstall $InstallName? (y/n)"
    if ($response -ne 'y' -and $response -ne 'Y') {
        Write-Host "Uninstall cancelled." -ForegroundColor Yellow
        pause
        exit 0
    }
    Write-Host ""
}

# Remove installation directory
Write-Host "Removing installation files..." -ForegroundColor Yellow
try {
    Remove-Item -Path $InstallPath -Recurse -Force
    Write-Host "  ✓ Removed: $InstallPath" -ForegroundColor Green
} catch {
    Write-Host "  ✗ Error removing directory: $_" -ForegroundColor Red
}
Write-Host ""

# Remove from system PATH
Write-Host "Updating system PATH..." -ForegroundColor Yellow
$currentPath = [Environment]::GetEnvironmentVariable("Path", "Machine")

if ($currentPath -like "*$InstallPath*") {
    # Remove the path (handle both ;path and path; cases)
    $newPath = $currentPath -replace [regex]::Escape($InstallPath + ";"), ""
    $newPath = $newPath -replace [regex]::Escape(";" + $InstallPath), ""
    $newPath = $newPath -replace [regex]::Escape($InstallPath), ""
    
    [Environment]::SetEnvironmentVariable("Path", $newPath, "Machine")
    Write-Host "  ✓ Removed from system PATH" -ForegroundColor Green
} else {
    Write-Host "  ✓ Not found in system PATH" -ForegroundColor Green
}
Write-Host ""

# Verify removal
if (-not (Test-Path $InstallPath)) {
    Write-Host "==========================================" -ForegroundColor Green
    Write-Host " Uninstall Successful!" -ForegroundColor Green
    Write-Host "==========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "$InstallName has been completely removed from your system." -ForegroundColor White
    Write-Host ""
} else {
    Write-Host "WARNING: Some files may still remain at:" -ForegroundColor Yellow
    Write-Host "  $InstallPath" -ForegroundColor White
    Write-Host "You may need to remove them manually." -ForegroundColor Yellow
    Write-Host ""
}

if (-not $Unattended) {
    pause
}
