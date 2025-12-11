# MikroC USB HID Bootloader - PC Host Tool

A cross-platform USB HID bootloader client for PIC32MZ microcontrollers running MikroElektronika's USB HID bootloader firmware. Automatically adapts to any PIC32MZ variant (1MB, 2MB) by reading device capabilities and hex file structure.

## Features

- **Fully Dynamic**: No hardcoded memory sizes - adapts to any PIC32MZ variant
- **Intel Hex Parsing**: Processes XC32-generated hex files with automatic address mapping
- **USB HID Protocol**: Complete MikroElektronika bootloader protocol implementation
- **Cross-Platform**: Windows (MinGW64) and Linux support
- **Smart Boot Vector Management**: Preserves original reset vectors from hex file

## Quick Start

### Windows
```bash
mikro_hb.exe firmware.hex
```

### Linux
```bash
mikro_hb firmware.hex
```

## Installation

### Windows

1. Download `mikro_hb-windows.zip` from releases
2. Extract and run `install.ps1` as Administrator
3. Installs to `C:\Program Files\MikroHB\` and adds to PATH

**Uninstall:**
```powershell
.\uninstall.ps1  # Run as Administrator
```

### Linux

1. Download `mikro_hb-linux.tar.gz` from releases
2. Extract and run:
```bash
sudo ./install.sh
```
Installs to `/usr/local/bin` and sets up USB permissions.

## How It Works

### Boot Sequence Architecture

The MikroC bootloader uses a three-vector boot system:

**1. Reset Vector (0xBFC00000 - Config Flash)**
```
MCLR Reset → Config Flash (0x1FC00000) → Jump to bootloader at 0x9D000D38
```

**2. Bootloader Decision Point**
```
Bootloader checks:
- Button pressed? Stay in bootloader mode
- USB enumeration? Stay in bootloader mode  
- Timeout? Jump to Boot Flash Vector (0xBFC00010)
```

**3. Boot Flash Vector (0xBFC00010 - Last page of Program Flash)**
```
Boot Flash (0x1D0F3FF0) → Contains reset vector from hex file → Jump to application
```

### Memory Map (PIC32MZ1024EFH - 1MB Example)

```
Virtual (KSEG0)    Physical         Size      Purpose
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
0x9D000000        0x1D000000       ~1MB      Program Flash
                  └─ 0x00           Varies    Application code/data
                                              (Reset vector: jump to app entry)

0x9D0F0000        0x1D0F0000       16KB      Boot Flash Page  
                  ├─ 0x0000         16368B    Padding (0xFF)
                  └─ 0x3FF0         16B       Reset vector from hex file
                                              (Typically jumps to 0xBFC00010)

0xBFC00000        0x1FC00000       2KB       Config Flash
                  ├─ 0x00           16B       Bootloader jump vector
                  │                           (Jump to 0x9D000D38)
                  ├─ 0x10+          Varies    Application data
                  └─ 0xFFC0         64B       Configuration bits
```

### Programming Sequence

The bootloader programs three memory regions in sequence:

**Region 1: Program Flash (0x1D000000)**
```
1. SYNC  - Synchronize with device
2. ERASE - Erase program flash blocks
3. WRITE - Send size to write
4. DATA  - Stream 64-byte packets (no command byte prefix)
```

**Region 2: Boot Flash Page (0x1D0F0000)**  
```
1. SYNC  - Synchronize
2. ERASE - Erase boot flash page (16KB)
3. WRITE - Send 16KB size
4. DATA  - Stream 256 packets of 0xFF
           Last packet contains reset vector from hex file (conf_ptr)
```

**Region 3: Config Flash (0x1FC00000)**
```
1. SYNC  - Synchronize  
2. ERASE - Erase config flash
3. WRITE - Send size
4. DATA  - Stream config data with bootloader jump vector (boot_line)
```

**Completion:**
```
5. REBOOT - Reset device to start application
```

### Critical Implementation: Boot Flash Reset Vector

The boot flash reset vector is extracted from the **config flash section** of the hex file (address 0x1FC00000):

```c
// Hex file contains reset vector at 0x1FC00000
// Example: c0bf1a3c10005a270800400300000000
//   lui k0, 0xBFC0
//   addiu k0, k0, 0x0010  
//   jr k0
//   nop

void overwrite_bootflash_program(void)
{
    // Fill with 0xFF for 16KB - 16 bytes
    for (i = 0; i < (0x4000 - 16); i++)
        *(prg_ptr++) = 0xff;
    
    // Copy reset vector from config flash data (conf_ptr_start)
    // This places it at offset 0x3FF0 in the boot flash page
    memcpy(prg_ptr, conf_ptr_start, 16);
}
```

**Why this works:**
- XC32 compiler generates a reset vector at 0x1FC00000 in the hex file
- This vector typically jumps to 0xBFC00010 (boot flash)
- We copy this vector to the boot flash page at offset 0x3FF0
- Physical address 0x1D0F3FF0 maps to virtual address 0xBFC00010
- When bootloader times out, it jumps here and the application starts

### Device Compatibility

The bootloader is fully dynamic and supports any PIC32MZ device:

**Detected at runtime from device:**
- `ulMcuSize` - Flash size (0x100000 for 1MB, 0x200000 for 2MB)
- `uiWriteBlock` - Write block size
- `uiEraseBlock` - Erase block size (typically 0x4000)

**Calculated dynamically:**
```c
// Boot flash location varies by device size
_boot_flash_start = 0x1D000000 + (ulMcuSize - 0x10000);
// For 1MB: 0x1D000000 + 0xF0000 = 0x1D0F0000
// For 2MB: 0x1D000000 + 0x1F0000 = 0x1D1F0000

// Config flash bootloader vector varies by device
if (ulMcuSize == 0x200000) // 2MB
    bootloader_address = 0xBD1F4000;  // Jump to bootloader
else // 1MB
    bootloader_address = 0xBD0F4000;  // Jump to bootloader
```

## Protocol Details

### USB HID Command Structure

All commands use 64-byte HID packets:

```
Byte 0:    0x0F (Start marker)
Byte 1:    Command code
Bytes 2-7: Command parameters (address, size, etc.)
Bytes 8+:  Padding with 0x00
```

### Command Reference

| Cmd | Code | Parameters | Description |
|-----|------|------------|-------------|
| INFO | 0x02 | None | Query device capabilities |
| BOOT | 0x03 | None | Enter bootloader mode |
| REBOOT | 0x04 | None | Reset device |
| SYNC | 0x15 | addr[4], size[2] | Prepare for operation |
| ERASE | 0x0B | addr[4], pages[2] | Erase flash region |
| WRITE | 0x0A | addr[4], size[2] | Initiate write operation |

**Data Streaming (State 3):**
After WRITE command, data is sent in 64-byte packets without command prefix.

### Intel HEX File Format

The bootloader parses XC32-generated Intel HEX files:

**Record Structure:**
```
:LLAAAATTDDDDDDCC
 ││ │  ││   │  └─ Checksum
 ││ │  ││   └──── Data bytes
 ││ │  │└──────── Record type (00=Data, 04=Extended Address, 01=EOF)
 ││ └──└───────── Address (16-bit)
 └└───────────── Length (bytes)
```

**Important Record Types:**
- `04`: Extended Linear Address - sets upper 16 bits of address
- `00`: Data Record - contains program data
- `01`: End of File

**Example:**
```hex
:020000041D00DD              ← Set base address to 0x1D000000
:10000000009D1A3C38085A270800400300000000F1  ← 16 bytes at 0x1D000000
:020000041FC01B              ← Set base address to 0x1FC00000  
:10000000C0BF1A3C10005A27080040030000000041  ← 16 bytes at 0x1FC00000
:00000001FF                  ← End of file
```

### Data Conditioning Process

The bootloader processes hex files in a single pass:

1. **Allocate Buffers** - Based on device flash size from INFO command
2. **Parse Records** - Extract address and data from each record
3. **Map to Offsets** - Calculate buffer offset from physical address
4. **Fill Gaps** - Unspecified regions filled with 0xFF
5. **Track Ranges** - Record min/max addresses for each region
6. **Calculate Sizes** - Determine exact byte count for programming

All memory regions start as 0xFF (erased flash state). Data from hex file overwrites specific offsets.

## Building from Source

### Prerequisites

**Windows (MSYS2/MinGW64):**
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-libusb
```

**Linux:**
```bash
# Debian/Ubuntu
sudo apt-get install build-essential libusb-1.0-0-dev

# Fedora/RHEL  
sudo dnf install gcc libusb-devel

# Arch
sudo pacman -S base-devel libusb
```

### Build

```bash
make          # Build binary
make clean    # Clean build artifacts
```

**Output:**
- Windows: `bins/mikro_hb.exe`
- Linux: `bins/mikro_hb`

### Debug Mode

Enable detailed logging by editing `srcs/HexFile.c`:

```c
#define DEBUG_PRINT 1  // Enable debug output
```

Debug output shows:
- Hex file parsing details
- Memory allocation
- Reset vector extraction
- Programming progress

## Troubleshooting

**Device not found:**
- Ensure device is in bootloader mode (press button during reset)
- Check USB connection
- On Linux: Verify udev rules are installed

**Programming fails:**
- Verify hex file is for correct PIC32MZ variant
- Check device is not write-protected
- Ensure bootloader firmware is installed on device

**Application doesn't start:**
- Verify hex file was compiled correctly with XC32
- Check that reset vectors are present in hex file
- Enable DEBUG_PRINT to verify reset vector extraction

## References

- [PIC32MZ Datasheet](http://ww1.microchip.com/downloads/en/DeviceDoc/PIC32MZ-EF-Family-Datasheet-DS60001320G.pdf)
- [Intel HEX Format Specification](https://en.wikipedia.org/wiki/Intel_HEX)
- [MikroElektronika USB HID Bootloader](https://www.mikroe.com/)

## License

Open source - free to use and modify.

---

**Important:** This tool programs PIC32MZ devices that already have MikroElektronika's USB HID bootloader firmware installed. It does NOT install the bootloader itself - that requires a hardware programmer (PICkit4, ICD4, etc.).