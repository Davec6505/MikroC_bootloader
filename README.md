# MikroC USB HID Bootloader - PC Host Tool

A fully dynamic USB HID bootloader client for PIC32MZ microcontrollers running MikroElektronika's USB HID bootloader firmware.

## Features

- **Fully Dynamic**: No hardcoded memory sizes - automatically adapts to any PIC32MZ variant (512K, 1024K, 2048K)
- **Intel Hex Parsing**: Intelligently processes hex files with automatic address conditioning
- **USB HID Protocol**: Complete implementation of MikroElektronika's bootloader protocol
- **Cross-Platform**: Works on Linux and Windows (MinGW64)

## Installation

### Windows - Easy Install (Recommended)

**Option 1: Using dist/ folder (for developers)**
1. Navigate to `dist/windows/` in this repository
2. Right-click `install.ps1` and select **"Run with PowerShell"** (as Administrator)
3. The installer will:
   - Copy files to `C:\Program Files\MikroHB\`
   - Add to system PATH
   - No additional dependencies needed!

**Option 2: Using release package**
1. Download the latest release package: `mikro_hb-windows.zip`
2. Extract the zip file
3. Right-click `install.ps1` and select **"Run with PowerShell"** (as Administrator)
4. The installer will:
   - Copy files to `C:\Program Files\MikroHB\`
   - Add to system PATH
   - No additional dependencies needed!

**Uninstall:**
```powershell
# Run as Administrator
.\uninstall.ps1
```

### Linux - Easy Install

**Option 1: Using dist/ folder (for developers)**
1. Navigate to `dist/linux/` in this repository
2. Run installer:
```bash
cd dist/linux
sudo ./install.sh
```

**Option 2: Using release package**
1. Download the latest release package: `mikro_hb-linux.tar.gz`
2. Extract and run installer:
```bash
tar -xzf mikro_hb-linux.tar.gz
cd mikro_hb-linux
sudo ./install.sh
```

The installer will:
- Install to `/usr/local/bin`
- Install libusb-1.0 dependencies (via package manager)
- Set up USB permissions (udev rules)

## Quick Start

### Windows
```bash
mikro_hb.exe firmware.hex
```

### Linux
```bash
mikro_hb firmware.hex
```

## Building from Source

### Windows (MinGW64/MSYS2)

**Prerequisites:**
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-libusb
```

**Build:**
```bash
make
```

**Package for distribution:**
```bash
./scripts/package-windows.sh
```

### Linux

**Prerequisites:**
```bash
# Debian/Ubuntu
sudo apt-get install build-essential libusb-1.0-0-dev

# Fedora/RHEL
sudo dnf install gcc libusb-devel

# Arch Linux
sudo pacman -S base-devel libusb
```

**Build:**
```bash
make
```

## How It Works

The bootloader communicates with PIC32MZ devices running MikroElektronika's USB HID bootloader firmware. The process is fully dynamic and requires no device-specific configuration.
  
    


/////////////////////////////////////////////////////////////////////////


### Programming Sequence

1. **SYNC** - Establish communication
2. **INFO** - Query device capabilities (flash size, write block size, etc.)
3. **BOOT** - Enter bootloader mode
4. **ERASE** - Erase flash region
5. **WRITE** - Stream hex data in 64-byte packets
6. Repeat steps 4-5 for each memory region (Program Flash, Boot Vector, Config Flash)

### Memory Region Processing (Example: PIC32MZ1024 - 1MB Flash)

The bootloader processes three distinct memory regions:

#### 1. Program Flash Region
- **Address Range**: `0x9D000000` (virtual) → `0x1D000000` (physical)
- **Size**: Dynamically determined from hex file
- **Purpose**: Application code and data
- **Example**: 2048 bytes = 32 packets of 64 bytes

#### 2. Boot Vector Page
- **Address Range**: `0x9D0F0000` (virtual) → `0x1D0F0000` (physical)
- **Size**: 16KB (0x4000 bytes) = 256 packets
- **Purpose**: Contains default boot vector at offset 0x3FF0
- **Content**: 
  - Filled with 0xFF
  - Last 16 bytes: Default PIC32 boot vector (jumps to 0xBFC00050)

#### 3. Config Flash Region
- **Address Range**: `0xBFC00000` (virtual) → `0x1FC00000` (physical)
- **Size**: 6144 bytes (3 write blocks) = 96 packets
- **Purpose**: Startup code and configuration bits
- **Content**:
  - Offset 0x00 (4 bytes): Application's first instruction (from program flash)
  - Offset 0x04-0x3F (60 bytes): NOP instructions (0x70000000)
  - Offset 0x40 (16 bytes): Boot vector to bootloader (jumps to 0xBD0F4000)
  - Remaining: Configuration data or 0xFF

### Physical Memory Map (PIC32MZ1024)

```
Physical Address          Region                Size        Purpose
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
0x1D000000              Program Flash          Variable    Application code
                        ...
0x1D0F0000              Boot Vector Page       16 KB       Boot redirect
0x1D0F3FF0                └─ Boot Vector       16 bytes    Jump to 0xBFC00050
                        ...
0x1FC00000              Config Flash           6 KB        Startup + config
0x1FC00000                ├─ First Instr       4 bytes     App entry point
0x1FC00004                ├─ NOPs              60 bytes    Padding
0x1FC00040                ├─ Boot Vector       16 bytes    Jump to 0xBD0F4000
0x1FC00050                └─ Config Data       Variable    Device config
```

### Critical Implementation Details

#### Boot Vector (Default PIC32 Reset)
```c
// Located at boot vector page offset 0x3FF0
// Jumps to default boot flash (0xBFC00050)
uint8_t default_boot_vector[16] = {
    0xC0, 0xBF, 0x1E, 0x3C,  // lui $30, 0xBFC0
    0x50, 0x00, 0xDE, 0x37,  // ori $30, $30, 0x0050  
    0x08, 0x00, 0xC0, 0x03,  // jr $30
    0x00, 0x00, 0x00, 0x70   // nop (delay slot)
};
```

#### Bootloader Boot Vector (Reset Handler)
```c
// Located at config flash offset 0x40 (64 bytes)
// Allows device to re-enter bootloader after reset
uint8_t bootloader_boot_vector[16] = {
    0x0F, 0xBD, 0x1E, 0x3C,  // lui $30, 0xBD0F
    0x00, 0x40, 0xDE, 0x37,  // ori $30, $30, 0x4000
    0x08, 0x00, 0xC0, 0x03,  // jr $30
    0x00, 0x00, 0x00, 0x70   // nop (delay slot)
};
```

#### Config Flash First Instruction
The config flash must begin with the application's first instruction to ensure proper startup:
```c
// Saved before boot vector processing corrupts buffer
memcpy(first_instruction, prg_ptr_start, 4);

// Later restored to config flash
memcpy(conf_ptr, first_instruction, 4);
```

### Boot Sequence

**First Boot (After Programming):**
1. **Power-on** → CPU starts at 0x1FC00000 (config flash offset 0)
2. **Execute first instruction** → Jump to program flash (application entry)
3. **Program flash begins executing** → Application runs
4. **LED blinks!** ✓

**Reset to Bootloader:**
1. **Reset button pressed** → CPU executes config flash offset 0x40
2. **Boot vector executed** → Jump to bootloader at 0xBD0F4000
3. **Bootloader runs** → USB HID device enumerates
4. **Ready for new firmware** ✓

This dual-vector approach allows both application execution AND bootloader re-entry without external hardware.

## Protocol Details

### USB HID Command Structure

```
<STX[0]><CMD_CODE[0]><ADDRESS[0..3]><COUNT[0..1]><DATA[0..COUNT-1]>
```

### Command Table

| Command | Format | Description |
|---------|--------|-------------|
| SYNC | `<STX><cmdSYNC>` | Synchronize with PC |
| INFO | `<STX><cmdINFO>` | Get device info (flash size, etc.) |
| BOOT | `<STX><cmdBOOT>` | Enter bootloader mode |
| ERASE | `<STX><cmdERASE><ADDR><COUNT>` | Erase flash blocks |
| WRITE | `<STX><cmdWRITE><ADDR><COUNT><DATA>` | Write flash data |
| REBOOT | `<STX><cmdREBOOT>` | Restart MCU |

### Dynamic Device Detection

The bootloader queries the device via `cmdINFO` to get:
- `ulMcuSize` - Total flash size (512K, 1024K, 2048K)
- `uiWriteBlock` - Write block size
- `uiEraseBlock` - Erase block size
- `uiMaxPacketSize` - USB packet size (64 bytes)

All memory calculations are based on these device-reported values, making the tool compatible with any PIC32MZ variant.

## Intel HEX File Format

### Record Structure

Intel HEX files are ASCII text files containing hexadecimal records. Each line follows this format:

```
:LLAAAATTDDDDDDDDDDCC

:        - Start code (colon)
LL       - Record length (number of data bytes)
AAAA     - Load address (16-bit)
TT       - Record type
DD...DD  - Data bytes
CC       - Checksum (2's complement of sum of all bytes)
```

### Record Types

| Type | Name | Description |
|------|------|-------------|
| `00` | Data Record | Contains program data |
| `01` | End of File | Last record in file |
| `04` | Extended Linear Address | Upper 16 bits of 32-bit address |
| `05` | Start Linear Address | Program entry point (optional) |

### Example PIC32MZ HEX File

```hex
:020000041D00DD              ← Extended address: 0x1D000000 (program flash)
:1000000027BDFFFC12F60400700000007000000046  ← 16 bytes @ 0x1D000000
:10001000700000007000000070000000700000007C  ← 16 bytes @ 0x1D000010
:020000041FC01B              ← Extended address: 0x1FC00000 (config flash)
:1000000027BDFFFC00000070000000700000007016  ← 16 bytes @ 0x1FC00000
:00000001FF                  ← End of file
```

**Explanation:**
- `:020000041D00DD` sets upper address to `0x1D00` → full address becomes `0x1D00xxxx`
- `:1000000027BDFFFC...` writes 16 (`0x10`) bytes starting at offset `0x0000`
- Combined: Data goes to physical address `0x1D000000`

### Address Translation

The bootloader handles PIC32's dual address spaces:

| Virtual (KSEG0) | Physical | Region |
|-----------------|----------|--------|
| `0x9D000000` | `0x1D000000` | Program Flash |
| `0x9D0F0000` | `0x1D0F0000` | Boot Vector Page |
| `0xBFC00000` | `0x1FC00000` | Config Flash |

The hex file uses physical addresses (`0x1D...`, `0x1FC...`), which the bootloader uses directly.

## Hex File Processing

The bootloader intelligently parses Intel HEX files:
1. **Reads Extended Linear Address (Type 04)** - Identifies memory region
2. **Sorts records by address** - Ensures linear programming
3. **Maps addresses to buffer offsets** - Handles non-contiguous data
4. **Fills gaps with 0xFF** - Erased flash default value
5. **Extracts first instruction** - Saved for config flash startup
6. **Calculates exact packet counts** - Based on actual data span

No assumptions about hex file structure - fully dynamic parsing based on record content.

## Debugging

### Debug Output Mode

The bootloader supports two output modes controlled by the `DEBUG_PRINT` define in `srcs/HexFile.c`:

**Progress Bar Mode (Default - DEBUG_PRINT 0):**
```
Programming: [========================================] 100%
```
Provides a clean visual progress indicator during programming.

**Debug Print Mode (DEBUG_PRINT 1):**
```
Saved first_instruction: fc ff bd 27
Using first_instruction: fc ff bd 27
```
Displays detailed information about config flash setup for debugging.

To enable debug mode:
1. Open `srcs/HexFile.c`
2. Change `#define DEBUG_PRINT 0` to `#define DEBUG_PRINT 1`
3. Rebuild with `make`

## Building

### Prerequisites

**Windows:**
- MinGW64 (MSYS2)
- libusb-1.0

**Linux:**
- GCC
- libusb-1.0-dev

### Compile
```bash
make
```

### Output
- Windows: `bins/mikro_hb.exe`
- Linux: `bins/mikro_hb`

## References

- [PIC32MZ Datasheet](http://www.microchip.com/downloads/en/devicedoc/60001115h.pdf)
- [Intel HEX Format](https://en.wikipedia.org/wiki/Intel_HEX)
- MikroElektronika USB HID Bootloader Firmware

## License

Open source - use freely.

---

**Note**: This tool requires the target PIC32MZ to be running MikroElektronika's USB HID bootloader firmware. The tool does NOT flash the bootloader itself - that must be done via a hardware programmer (PICkit, etc.).

we see that the number and the address are both sent as little endian 
shorts.

	:040EA8008080D6224E
	:10054600907FE9E070030206E114700302075D2460

	04 00 a8 0e 00 80 80 d6 22 4e 10 05 46 00 90 7f
	e9 e0 70 03 02 06

	10 00 46 05 00 90 7f e9 e0 70 03 02 06 e1 14 70
	03 02 07 5d 24 60

short records are padded with (random?) stuff, namely the start of the 
following record. 

////////////////////////////////////////////////////////////////////////////
//Last line of a hex file is always
:00000001FF	



///////////////////////////////////////////////////////////////////////////
//                            CODE AS IT STANDS                          //
///////////////////////////////////////////////////////////////////////////

In it current state this code is very efficient.
: The sequence is as follows,
  1) Condition the data array from hex file.
  2) Program Flash loaded first, quantity must be multiples of Row size,
     MCU specific, which is sent down from on chip firmware.
  3) Program BootStart up code.
  4) Program Config program
   
: The code iterates through each line of the hex file and uses the address
  LSW to offset the data array, it then uses the data quantity to load
  the hex value by iterating through each item in the row.
  
:On response from chip it uses memory size to determine the last 16bytes
  of the last page for the start up vector jump.


///////////////////////////////////////////////////////////////////////////
//TODO

 :Strip down the hex file more efficiently ??  
 
 == Done, only iterate through file once now, each
    Address encountered in the hex file offsets the 
    data position within the data array.
 
 :Understanding how to flash large programs? must they be
  page allocated / mcu told that a new page is on its way?