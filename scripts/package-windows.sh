#!/bin/bash
# MikroC USB HID Bootloader - Windows Package Builder
# Collects exe and DLLs from MinGW64 for distribution

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DIST_DIR="$PROJECT_ROOT/dist/windows"
BIN_DIR="$PROJECT_ROOT/bins"

# MinGW64 DLL locations (adjust if your MinGW64 is in a different location)
MINGW_BIN="/mingw64/bin"

# Required files
EXE_FILE="mikro_hb.exe"
REQUIRED_DLLS=(
    "libusb-1.0.dll"
    "libgcc_s_seh-1.dll"
    "libwinpthread-1.dll"
)

echo "=========================================="
echo " MikroC Bootloader - Windows Packager"
echo "=========================================="
echo ""

# Check if binary exists
if [ ! -f "$BIN_DIR/$EXE_FILE" ]; then
    echo "ERROR: $EXE_FILE not found in $BIN_DIR"
    echo "Please run 'make' first to build the project."
    exit 1
fi

echo "✓ Found: $EXE_FILE"

# Check MinGW64 bin directory
if [ ! -d "$MINGW_BIN" ]; then
    echo ""
    echo "WARNING: MinGW64 bin directory not found at: $MINGW_BIN"
    echo "Please edit this script to set the correct MINGW_BIN path."
    echo ""
    echo "Common locations:"
    echo "  /mingw64/bin"
    echo "  C:/msys64/mingw64/bin"
    echo "  /c/msys64/mingw64/bin"
    exit 1
fi

echo "✓ Found MinGW64 at: $MINGW_BIN"
echo ""

# Check for required DLLs
echo "Checking for required DLLs..."
MISSING_DLLS=()
for dll in "${REQUIRED_DLLS[@]}"; do
    if [ ! -f "$MINGW_BIN/$dll" ]; then
        MISSING_DLLS+=("$dll")
        echo "  ✗ Missing: $dll"
    else
        echo "  ✓ Found: $dll"
    fi
done

if [ ${#MISSING_DLLS[@]} -gt 0 ]; then
    echo ""
    echo "ERROR: Missing required DLLs from MinGW64:"
    for dll in "${MISSING_DLLS[@]}"; do
        echo "  - $dll"
    done
    echo ""
    echo "Please install missing dependencies with:"
    echo "  pacman -S mingw-w64-x86_64-libusb"
    exit 1
fi

echo ""
echo "Packaging Windows distribution..."
echo ""

# Copy executable
echo "Copying $EXE_FILE..."
cp "$BIN_DIR/$EXE_FILE" "$DIST_DIR/"
echo "  ✓ $DIST_DIR/$EXE_FILE"

# Copy DLLs
echo "Copying DLLs..."
for dll in "${REQUIRED_DLLS[@]}"; do
    cp "$MINGW_BIN/$dll" "$DIST_DIR/"
    echo "  ✓ $DIST_DIR/$dll"
done

# Copy README
if [ -f "$PROJECT_ROOT/README.md" ]; then
    cp "$PROJECT_ROOT/README.md" "$DIST_DIR/"
    echo "  ✓ $DIST_DIR/README.md"
fi

echo ""
echo "=========================================="
echo " Package Complete!"
echo "=========================================="
echo ""
echo "Distribution files ready at:"
echo "  $DIST_DIR"
echo ""
echo "Contents:"
ls -lh "$DIST_DIR" | grep -E '\.(exe|dll|ps1|md)$' || ls -lh "$DIST_DIR"
echo ""
echo "To create a release package:"
echo "  cd $DIST_DIR"
echo "  zip -r mikro_hb-windows.zip *.exe *.dll *.ps1 README.md"
echo ""
