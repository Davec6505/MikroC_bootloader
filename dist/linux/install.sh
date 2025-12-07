#!/bin/bash
# MikroC USB HID Bootloader - Linux Installer
# Installs mikro_hb to /usr/local/bin and libusb dependencies

set -e

# Installation configuration
INSTALL_DIR="/usr/local/bin"
BIN_NAME="mikro_hb"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=========================================="
echo " MikroC USB HID Bootloader - Installer"
echo "=========================================="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "ERROR: This installer requires root privileges."
    echo "Please run with sudo:"
    echo "  sudo $0"
    echo ""
    exit 1
fi

# Verify binary exists
if [ ! -f "$SCRIPT_DIR/$BIN_NAME" ]; then
    echo "ERROR: $BIN_NAME not found in $SCRIPT_DIR"
    echo "Please ensure the binary is in the same directory as this installer."
    echo ""
    exit 1
fi

echo "✓ Found: $BIN_NAME"
echo ""

# Detect package manager and install libusb
echo "Checking for libusb-1.0..."

if command -v apt-get &> /dev/null; then
    # Debian/Ubuntu
    echo "Detected: Debian/Ubuntu (apt)"
    if ! dpkg -l | grep -q libusb-1.0-0; then
        echo "Installing libusb-1.0-0..."
        apt-get update
        apt-get install -y libusb-1.0-0
        echo "  ✓ Installed libusb-1.0-0"
    else
        echo "  ✓ libusb-1.0-0 already installed"
    fi
elif command -v dnf &> /dev/null; then
    # Fedora/RHEL
    echo "Detected: Fedora/RHEL (dnf)"
    if ! rpm -q libusbx &> /dev/null; then
        echo "Installing libusbx..."
        dnf install -y libusbx
        echo "  ✓ Installed libusbx"
    else
        echo "  ✓ libusbx already installed"
    fi
elif command -v pacman &> /dev/null; then
    # Arch Linux
    echo "Detected: Arch Linux (pacman)"
    if ! pacman -Q libusb &> /dev/null; then
        echo "Installing libusb..."
        pacman -S --noconfirm libusb
        echo "  ✓ Installed libusb"
    else
        echo "  ✓ libusb already installed"
    fi
elif command -v zypper &> /dev/null; then
    # openSUSE
    echo "Detected: openSUSE (zypper)"
    if ! rpm -q libusb-1_0-0 &> /dev/null; then
        echo "Installing libusb-1_0-0..."
        zypper install -y libusb-1_0-0
        echo "  ✓ Installed libusb-1_0-0"
    else
        echo "  ✓ libusb-1_0-0 already installed"
    fi
else
    echo "WARNING: Could not detect package manager."
    echo "Please install libusb-1.0 manually for your distribution."
    echo ""
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo ""

# Install binary
echo "Installing $BIN_NAME to $INSTALL_DIR..."
cp "$SCRIPT_DIR/$BIN_NAME" "$INSTALL_DIR/$BIN_NAME"
chmod +x "$INSTALL_DIR/$BIN_NAME"
echo "  ✓ Installed: $INSTALL_DIR/$BIN_NAME"
echo ""

# Set up udev rules for USB access without sudo
UDEV_RULES_FILE="/etc/udev/rules.d/99-mikrohb.rules"
echo "Setting up USB permissions (udev rules)..."

cat > "$UDEV_RULES_FILE" << 'EOF'
# MikroElektronika USB HID Bootloader
# Allows non-root access to bootloader devices
SUBSYSTEM=="usb", ATTR{idVendor}=="2dbc", ATTR{idProduct}=="0001", MODE="0666", GROUP="plugdev"
EOF

chmod 644 "$UDEV_RULES_FILE"
echo "  ✓ Created: $UDEV_RULES_FILE"

# Reload udev rules
if command -v udevadm &> /dev/null; then
    udevadm control --reload-rules
    udevadm trigger
    echo "  ✓ Reloaded udev rules"
fi

echo ""

# Verify installation
if [ -x "$INSTALL_DIR/$BIN_NAME" ]; then
    echo "=========================================="
    echo " Installation Successful!"
    echo "=========================================="
    echo ""
    echo "You can now use '$BIN_NAME' from any terminal."
    echo ""
    echo "Usage examples:"
    echo "  mikro_hb firmware.hex"
    echo "  mikro_hb --serial /dev/ttyUSB0 --baud 115200 firmware.hex"
    echo ""
    echo "NOTE: USB permissions are configured for the 'plugdev' group."
    echo "If you get permission errors, add your user to plugdev:"
    echo "  sudo usermod -a -G plugdev \$USER"
    echo "  (log out and back in for changes to take effect)"
    echo ""
else
    echo "ERROR: Installation verification failed!"
    exit 1
fi
