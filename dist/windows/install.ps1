# MikroC USB HID Bootloader - Windows Installer
# Installs mikro_hb.exe to Program Files and adds to system PATH

param(
    [switch]$Unattended = $false
)

$ErrorActionPreference = "Stop"

# Installation configuration
$InstallName = "MikroHB"
$InstallPath = "$env:ProgramFiles\$InstallName"
$ExeName = "mikro_hb.exe"

# Required files to install
$RequiredFiles = @(
    "mikro_hb.exe",
    "libusb-1.0.dll",
    "libgcc_s_seh-1.dll",
    "libwinpthread-1.dll"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " MikroC USB HID Bootloader - Installer" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "ERROR: This installer requires Administrator privileges." -ForegroundColor Red
    Write-Host "Please right-click and select 'Run as Administrator'" -ForegroundColor Yellow
    Write-Host ""
    pause
    exit 1
}

# Get current script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Verify all required files exist
Write-Host "Checking required files..." -ForegroundColor Yellow
$missingFiles = @()
foreach ($file in $RequiredFiles) {
    $filePath = Join-Path $ScriptDir $file
    if (-not (Test-Path $filePath)) {
        $missingFiles += $file
    } else {
        Write-Host "  ✓ $file" -ForegroundColor Green
    }
}

if ($missingFiles.Count -gt 0) {
    Write-Host ""
    Write-Host "ERROR: Missing required files:" -ForegroundColor Red
    foreach ($file in $missingFiles) {
        Write-Host "  ✗ $file" -ForegroundColor Red
    }
    Write-Host ""
    pause
    exit 1
}

Write-Host ""

# Check if already installed
if (Test-Path $InstallPath) {
    Write-Host "WARNING: $InstallName is already installed at:" -ForegroundColor Yellow
    Write-Host "  $InstallPath" -ForegroundColor White
    Write-Host ""
    
    if (-not $Unattended) {
        $response = Read-Host "Do you want to overwrite the existing installation? (y/n)"
        if ($response -ne 'y' -and $response -ne 'Y') {
            Write-Host "Installation cancelled." -ForegroundColor Yellow
            pause
            exit 0
        }
    }
    
    Write-Host "Removing existing installation..." -ForegroundColor Yellow
    Remove-Item -Path $InstallPath -Recurse -Force
}

# Create installation directory
Write-Host "Creating installation directory..." -ForegroundColor Yellow
New-Item -ItemType Directory -Path $InstallPath -Force | Out-Null
Write-Host "  ✓ Created: $InstallPath" -ForegroundColor Green
Write-Host ""

# Copy files
Write-Host "Installing files..." -ForegroundColor Yellow
foreach ($file in $RequiredFiles) {
    $source = Join-Path $ScriptDir $file
    $dest = Join-Path $InstallPath $file
    Copy-Item -Path $source -Destination $dest -Force
    Write-Host "  ✓ Installed: $file" -ForegroundColor Green
}
Write-Host ""

# Add to system PATH if not already present
Write-Host "Updating system PATH..." -ForegroundColor Yellow
$currentPath = [Environment]::GetEnvironmentVariable("Path", "Machine")

if ($currentPath -notlike "*$InstallPath*") {
    $newPath = $currentPath + ";" + $InstallPath
    [Environment]::SetEnvironmentVariable("Path", $newPath, "Machine")
    Write-Host "  ✓ Added to system PATH" -ForegroundColor Green
} else {
    Write-Host "  ✓ Already in system PATH" -ForegroundColor Green
}
Write-Host ""

# Verify installation
$exePath = Join-Path $InstallPath $ExeName
if (Test-Path $exePath) {
    Write-Host "========================================" -ForegroundColor Green
    Write-Host " Installation Successful!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Installation location:" -ForegroundColor White
    Write-Host "  $InstallPath" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "You can now use '$ExeName' from any command prompt." -ForegroundColor White
    Write-Host ""
    Write-Host "Usage example:" -ForegroundColor Yellow
    Write-Host "  mikro_hb.exe firmware.hex" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "NOTE: You may need to restart your terminal for PATH changes to take effect." -ForegroundColor Yellow
    Write-Host ""
} else {
    Write-Host "ERROR: Installation verification failed!" -ForegroundColor Red
    exit 1
}

if (-not $Unattended) {
    pause
}
