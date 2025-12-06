# MikroC Bootloader Development Status
**Date**: December 6, 2025  
**Device**: PIC32MZ1048EFH064 (Clicker board)  
**Platform**: Windows MinGW64

---

## Current Status: ⚠️ BROKEN - Bootloader Firmware Corrupted

### Critical Issue
The **bootloader firmware on the MCU itself has been corrupted** during testing. The device no longer boots or enumerates on USB.

**Symptom**: After programming with `mikro_hb.exe`, the device:
- Does not execute application code (LED doesn't blink)
- May not enumerate as USB HID device
- Bootloader firmware is non-functional

**Root Cause**: Unknown - investigating:
1. Incorrect boot flash vector data sent to device
2. Wrong `boot_line[]` array selected for 1MB chip
3. Configuration bits corrupted
4. Memory region overlap or corruption
5. **FIXED**: Padding calculation - now tracks highest address to include 0xFF gaps

---

## ⚠️ MUST DO BEFORE CONTINUING

### Recovery Required

**CRITICAL: Reflash bootloader firmware using hardware programmer:**
1. Use PICkit 3/4, MPLAB ICD, or MPLAB SNAP
2. Flash original MikroElektronika USB HID bootloader `.hex` file
3. Restore device to working state
4. **THEN** test with updated code

**Required File**: MikroElektronika USB HID Bootloader firmware for PIC32MZ1048EFH064

---

## Latest Code Changes (Dec 6, 2025 - Session End)

### ✅ Just Fixed: Memory Padding Calculation

**File**: `srcs/HexFile.c` lines 433-447

**Problem**: `prg_mem_count` was tracking sum of bytes written, NOT including gaps
- Gaps between hex records were filled with 0xFF in buffer ✓
- But `prg_mem_count` didn't account for gap sizes ✗
- Result: Told device to write fewer bytes than actual buffer span

**Fix Applied**:
```c
// OLD (WRONG):
prg_byte_count = temp_prg_add - prg_mem_last;
prg_mem_count += prg_byte_count;  // Sum of bytes
prg_mem_last = temp_prg_add;

// NEW (CORRECT):
uint32_t end_address = temp_prg_add + prg_byte_count;
if (end_address > prg_mem_count)
    prg_mem_count = end_address;  // Highest address = includes gaps
```

**Result**: 
- `prg_mem_count` now represents total span from 0x00 to highest byte
- Includes all 0xFF-padded gaps
- Rounds correctly to 2KB write blocks (line 654)
- Removed unused variables `prg_mem_last`, `conf_mem_last`

**Status**: ✅ Compiled successfully, NOT TESTED (device corrupted)

---

## Work Completed This Session

### ✅ Completed Tasks

1. **USB Protocol Analysis** (`other/USB_PROTOCOL_ANALYSIS.md`)
   - Analyzed Wireshark PCAP capture
   - Documented command structure: cmdINFO, cmdBOOT, cmdERASE, cmdWRITE, cmdREBOOT
   - Identified packet format: 0x0F prefix + command + 62 bytes data
   - Memory regions: 0x1D000000 (Program), 0x1D0F0000 (Boot), 0x1FC00000 (Config)
   - ⚠️ Updated with warning: implementation broken, needs exact PCAP match

2. **Code Issues Documentation** (`other/CODE_ANALYSIS_ISSUES.md`)
   - Identified 6 critical issues in hardcoded bootloader
   - Documented vector index loop limitation
   - Memory allocation problems
   - Erase address calculation bug
   - ⚠️ Updated with corruption status

3. **Cross-Platform Support**
   - Added OS detection macros to all `.c` files
   - Builds on Windows (MinGW64) and Linux (GCC)
   - `#ifndef _WIN32 #include <linux/types.h> #endif`

4. **Progress Display**
   - Implemented `--verbose` flag for debug mode
   - Added `g_verbose_mode` global flag
   - Progress bar mode: `[========================================] 100%`
   - Debug mode: Shows TX/RX hex dumps

5. **Bug Fixes Applied**
   - **CRITICAL**: Fixed erase address calculation (line 679 `HexFile.c`)
     - Changed from: `_temp_flash_erase_ = (vector[vector_index]) + (blocks * size);`
     - Changed to: `_temp_flash_erase_ = (vector[vector_index]);`
   - This was sending END address (0x1D004000) instead of START address (0x1D000000)
   
   - **CRITICAL**: Fixed memory padding calculation (lines 433-447 `HexFile.c`)
     - Changed from cumulative byte count to highest address tracking
     - Ensures 0xFF gaps are included in total size for proper write block rounding

6. **Code Cleanup**
   - Removed unnecessary `HexFile_v2.c` implementation (agent's mistake)
   - Removed `--v2` flag from command line
   - Cleaned up `HexFile.h` to remove v2 function prototypes
   - Updated Makefile to remove v2 references
   - Removed unused variables: `prg_mem_last`, `conf_mem_last`

---

## Known Issues

### 🔴 BLOCKER: Device Won't Boot After Programming

**Symptoms:**
- `mikro_hb.exe` programs successfully (no USB errors)
- Progress shows 100% for all three regions
- Device reboots (cmdREBOOT returns -1 timeout, expected)
- **LED does NOT blink** - application code not running
- Device may not re-enumerate on USB

**Commands Sent (from --verbose):**
```
[TX] 0f 02 ... (cmdINFO)
[RX] 38 01 15 00 08 00 00 00 00 00 10 00 ... (Device reports 512KB - WRONG!)
[TX] 0f 03 ... (cmdBOOT)
[TX] 0f 15 00 00 00 1d 01 00 ... (cmdERASE at 0x1D000000) ✓ CORRECT
[TX] 0f 0b 00 00 00 1d 00 08 ... (cmdWRITE 2KB at 0x1D000000) ✓ CORRECT
[TX] fc ff bd 27 ... (Flash data packets)
...
[TX] 0f 04 ... (cmdREBOOT)
```

**Addresses match PCAP protocol analysis** ✓  
**Erase bug is FIXED** ✓  
**Padding bug is FIXED** ✓  
**But device bootloader is corrupted** ❌

---

## Suspected Root Causes (To Investigate After Recovery)

### 1. Boot Flash Vector Selection Issue
**File**: `srcs/HexFile.c` lines 610-616

```c
// offset decided on memory size of chip
if (bootinfo_t.ulMcuSize.fValue == MZ2048)
    memcpy(conf_ptr, boot_line[0], sizeof(boot_line[0]));  // 1F BD ...
else
    memcpy(conf_ptr, boot_line[1], sizeof(boot_line[1]));  // 0F BD ...
```

**Problem**: 
- Device reports `ulMcuSize.fValue = 0x080000` (512KB)
- Actual device is PIC32MZ1048 (1MB)
- Code selects `boot_line[1]` (0x0F BD...) but may need `boot_line[0]` (0x1F BD...)
- **Wrong boot vector = device won't jump to application code**

**boot_line arrays:**
```c
const uint8_t boot_line[][16] = {
    {0x1F, 0xBD, 0x1E, 0x3C, 0x00, 0x40, 0xDE, 0x37, 0x08, 0x00, 0xC0, 0x03, 0x00, 0x00, 0x00, 0x70},  // [0] for MZ2048
    {0x0F, 0xBD, 0x1E, 0x3C, 0x00, 0x40, 0xDE, 0x37, 0x08, 0x00, 0xC0, 0x03, 0x00, 0x00, 0x00, 0x70}   // [1] for others
};
```

Difference: First byte only (0x1F vs 0x0F)

### 2. PCAP Byte Mismatch
**Need to verify**: Every byte sent must match working PCAP capture exactly
- PCAP file: `other/mikroc_bootlaoder_pic32mz1024_blinky.pcapng`
- Text file `mikroc_bootloader_pic1024_usbcap.txt` contains USB camera data (WRONG FILE)
- Must export PCAP correctly or use tshark to extract USB data
- Compare TX data byte-by-byte with working capture

### 3. Configuration Flash Data
**Possible issue**: Config bits (0x1FC00000 region) may be incorrect
- Device configuration registers control:
  - Oscillator settings
  - Watchdog timer
  - Code protection
  - Boot mode
- Incorrect config = device won't execute

### 4. Incomplete Page Padding (NOW FIXED ✓)
**Was**: Pages may not be fully padded with 0xFF
- Buffer IS initialized with 0xFF (line 397, 404) ✓
- **FIXED**: `prg_mem_count` now tracks highest address, includes gaps ✓
- Rounds correctly to 2KB write blocks ✓

---

## Next Steps (After Bootloader Restored)

### Immediate Priority
1. ✅ **Restore bootloader firmware using hardware programmer** ← DO THIS FIRST
2. Test updated code with padding fix
3. Extract exact USB data from working PCAP
4. Compare our TX data byte-by-byte with PCAP
5. Identify any remaining differences

### Investigation Required
1. **Verify boot_line selection logic**
   - Check what MZ1024 vs MZ2048 actually needs
   - Test both boot_line[0] and boot_line[1]
   - May need to hardcode for PIC32MZ1048

2. **Validate MCU size reporting**
   - Device reports 0x080000 (512KB) 
   - Should report 0x100000 (1MB)
   - May be bootloader firmware bug

3. **Parse PCAP for ground truth**
   - Extract all TX packets from working capture
   - Create byte-for-byte comparison script
   - Find exact mismatches

4. **Test hex file data**
   - Verify `Clicker_Blinky.hex` is valid
   - Check reset vector address
   - Confirm entry point

---

## Code State

### Modified Files
- ✅ `srcs/HexFile.c` - Fixed erase address bug (line 679) + padding calculation (lines 433-447)
- ✅ `srcs/USB.c` - Added verbose mode and progress bar
- ✅ `srcs/MikroHB.c` - Added --verbose flag, removed --v2
- ✅ `srcs/Utils.c` - Added OS detection
- ✅ `incs/USB.h` - Added extern declarations
- ✅ `incs/HexFile.h` - Removed v2 prototypes
- ✅ `srcs/Makefile` - Removed HexFile_v2.c

### Deleted Files
- ✅ `srcs/HexFile_v2.c` - Unnecessary dynamic rewrite
- ✅ `objs/HexFile_v2.o` - Build artifact

### Build Status
```bash
make clean && make
```
✅ Compiles successfully on Windows MinGW64  
✅ Executable: `bins/mikro_hb.exe`  
⚠️ **NOT TESTED** - device bootloader corrupted

---

## Testing Commands (For After Recovery)

### Normal Mode (Progress Bar)
```powershell
.\bins\mikro_hb.exe "C:\Users\Public\Documents\Mikroelektronika\mikroC PRO for PIC32\Examples\Internal MCU modules\P32MZ2048EFH144\ClickerMZ_Blinky\srcs\Clicker_Blinky.hex"
```

### Debug Mode (Hex Dump)
```powershell
.\bins\mikro_hb.exe --verbose "C:\Users\Public\Documents\Mikroelektronika\mikroC PRO for PIC32\Examples\Internal MCU modules\P32MZ2048EFH144\ClickerMZ_Blinky\srcs\Clicker_Blinky.hex"
```

---

## Reference Data

### Memory Map (PIC32MZ1048EFH064)
- **Program Flash**: 0x1D000000 - 0x1D0FFFFF (1MB)
- **Boot Flash**: 0x1D0F0000 - 0x1D0FFFFF (64KB, overlaps end of program)
- **Config Flash**: 0x1FC00000 - 0x1FC02FFF (12KB)

### Flash Specifications
- **Erase Block**: 16KB (0x4000)
- **Write Block**: 2KB (0x0800)
- **USB Packet**: 64 bytes
- **Packets per Write**: 2048 / 64 = 32 packets

### Device Info (from cmdINFO response)
```
38 01 15 00 08 00 00 00 00 00 10 00 03 00 00 40 04 00 00 08 05 00 00 12 ...
```
Parsed:
- Erase Block: 0x4000 (16KB) ✓
- Write Block: 0x0800 (2KB) ✓
- MCU Size: 0x080000 (512KB) ❌ WRONG - Should be 0x100000

---

## Critical Path Forward

**BLOCKER**: Must restore bootloader firmware before any further testing

**When resuming**:
1. ✅ Device has been recovered with hardware programmer
2. Test updated code with both bug fixes (erase address + padding)
3. Use --verbose mode to capture TX data
4. Export PCAP data from `mikroc_bootlaoder_pic32mz1024_blinky.pcapng`
5. Byte-by-byte comparison to find any remaining differences
6. Fix boot_line selection if needed
7. Document working solution

---

## Notes

- Original code worked on **Linux** - now testing on **Windows**
- Cross-platform differences should be minimal (libusb-1.0)
- User confirmed original code was working, just had specific bugs
- **Do NOT rewrite** - fix bugs in original only
- HexFile_v2.c was agent's mistake - removed
- Two critical bugs fixed: erase address calculation + padding gaps
- Device bootloader corrupted - needs hardware recovery before continuing
- **Session paused** - will resume after device recovery

---

## Work Completed

### ✅ Completed Tasks

1. **USB Protocol Analysis** (`other/USB_PROTOCOL_ANALYSIS.md`)
   - Analyzed Wireshark PCAP capture
   - Documented command structure: cmdINFO, cmdBOOT, cmdERASE, cmdWRITE, cmdREBOOT
   - Identified packet format: 0x0F prefix + command + 62 bytes data
   - Memory regions: 0x1D000000 (Program), 0x1D0F0000 (Boot), 0x1FC00000 (Config)

2. **Code Issues Documentation** (`other/CODE_ANALYSIS_ISSUES.md`)
   - Identified 6 critical issues in hardcoded bootloader
   - Documented vector index loop limitation
   - Memory allocation problems
   - Erase address calculation bug

3. **Cross-Platform Support**
   - Added OS detection macros to all `.c` files
   - Builds on Windows (MinGW64) and Linux (GCC)
   - `#ifndef _WIN32 #include <linux/types.h> #endif`

4. **Progress Display**
   - Implemented `--verbose` flag for debug mode
   - Added `g_verbose_mode` global flag
   - Progress bar mode: `[========================================] 100%`
   - Debug mode: Shows TX/RX hex dumps

5. **Bug Fixes**
   - **CRITICAL**: Fixed erase address calculation (line 679 `HexFile.c`)
     - Changed from: `_temp_flash_erase_ = (vector[vector_index]) + (blocks * size);`
     - Changed to: `_temp_flash_erase_ = (vector[vector_index]);`
   - This was sending END address (0x1D004000) instead of START address (0x1D000000)

6. **Code Cleanup**
   - Removed unnecessary `HexFile_v2.c` implementation
   - Removed `--v2` flag from command line
   - Cleaned up `HexFile.h` to remove v2 function prototypes
   - Updated Makefile to remove v2 references

---

## Known Issues

### 🔴 BLOCKER: Device Won't Boot After Programming

**Symptoms:**
- `mikro_hb.exe` programs successfully (no USB errors)
- Progress shows 100% for all three regions
- Device reboots (cmdREBOOT returns -1 timeout, expected)
- **LED does NOT blink** - application code not running
- Device may not re-enumerate on USB

**Commands Sent (from --verbose):**
```
[TX] 0f 02 ... (cmdINFO)
[RX] 38 01 15 00 08 00 00 00 00 00 10 00 ... (Device reports 512KB - WRONG!)
[TX] 0f 03 ... (cmdBOOT)
[TX] 0f 15 00 00 00 1d 01 00 ... (cmdERASE at 0x1D000000) ✓ CORRECT
[TX] 0f 0b 00 00 00 1d 00 08 ... (cmdWRITE 2KB at 0x1D000000) ✓ CORRECT
[TX] fc ff bd 27 ... (Flash data packets)
...
[TX] 0f 04 ... (cmdREBOOT)
```

**Addresses match PCAP protocol analysis** ✓  
**Erase bug is FIXED** ✓  
**But device still doesn't boot** ❌

---

## Suspected Root Causes

### 1. Boot Flash Vector Selection Issue
**File**: `srcs/HexFile.c` lines 610-616

```c
// offset decided on memory size of chip
if (bootinfo_t.ulMcuSize.fValue == MZ2048)
    memcpy(conf_ptr, boot_line[0], sizeof(boot_line[0]));  // 1F BD ...
else
    memcpy(conf_ptr, boot_line[1], sizeof(boot_line[1]));  // 0F BD ...
```

**Problem**: 
- Device reports `ulMcuSize.fValue = 0x080000` (512KB)
- Actual device is PIC32MZ1048 (1MB)
- Code selects `boot_line[1]` (0x0F BD...) but may need `boot_line[0]` (0x1F BD...)
- **Wrong boot vector = device won't jump to application code**

**boot_line arrays:**
```c
const uint8_t boot_line[][16] = {
    {0x1F, 0xBD, 0x1E, 0x3C, 0x00, 0x40, 0xDE, 0x37, 0x08, 0x00, 0xC0, 0x03, 0x00, 0x00, 0x00, 0x70},  // [0]
    {0x0F, 0xBD, 0x1E, 0x3C, 0x00, 0x40, 0xDE, 0x37, 0x08, 0x00, 0xC0, 0x03, 0x00, 0x00, 0x00, 0x70}   // [1]
};
```

Difference: First byte only (0x1F vs 0x0F)

### 2. PCAP Byte Mismatch
**Need to verify**: Every byte sent must match working PCAP capture exactly
- PCAP file: `other/mikroc_bootlaoder_pic32mz1024_blinky.pcapng`
- Text file `mikroc_bootloader_pic1024_usbcap.txt` contains USB camera data (WRONG FILE)
- Must export PCAP correctly or use tshark to extract USB data

### 3. Configuration Flash Data
**Possible issue**: Config bits (0x1FC00000 region) may be incorrect
- Device configuration registers control:
  - Oscillator settings
  - Watchdog timer
  - Code protection
  - Boot mode
- Incorrect config = device won't execute

### 4. Incomplete Page Padding
**Possible issue**: Pages may not be fully padded with 0xFF
- Buffer IS initialized with 0xFF (line 397, 404) ✓
- But actual data written might not cover full erase blocks
- Flash write in pages must be 0xFF-padded to erase block boundary (16KB)
- **Critical**: Partial pages MUST be padded to write block size (2KB minimum)
- Hex file may have gaps that aren't being filled with 0xFF in the transmitted data

---

## Next Steps (After Bootloader Restored)

### Immediate Priority
1. ✅ Restore bootloader firmware using hardware programmer
2. Extract exact USB data from working PCAP
3. Compare our TX data byte-by-byte with PCAP
4. Identify the exact difference causing boot failure

### Investigation Required
1. **Verify boot_line selection logic**
   - Check what MZ1024 vs MZ2048 actually needs
   - Test both boot_line[0] and boot_line[1]

2. **Validate MCU size reporting**
   - Device reports 0x080000 (512KB) 
   - Should report 0x100000 (1MB)
   - May be bootloader firmware bug

3. **Parse PCAP for ground truth**
   - Extract all TX packets from working capture
   - Create byte-for-byte comparison script
   - Find exact mismatches

4. **Test hex file data**
   - Verify `Clicker_Blinky.hex` is valid
   - Check reset vector address
   - Confirm entry point

---

## Code State

### Modified Files
- ✅ `srcs/HexFile.c` - Fixed erase address bug (line 679)
- ✅ `srcs/USB.c` - Added verbose mode and progress bar
- ✅ `srcs/MikroHB.c` - Added --verbose flag, removed --v2
- ✅ `srcs/Utils.c` - Added OS detection
- ✅ `incs/USB.h` - Added extern declarations
- ✅ `incs/HexFile.h` - Removed v2 prototypes
- ✅ `srcs/Makefile` - Removed HexFile_v2.c

### Deleted Files
- ✅ `srcs/HexFile_v2.c` - Unnecessary dynamic rewrite
- ✅ `objs/HexFile_v2.o` - Build artifact

### Build Status
```bash
make clean && make
```
✅ Compiles successfully on Windows MinGW64  
✅ Executable: `bins/mikro_hb.exe`

---

## Testing Commands

### Normal Mode (Progress Bar)
```powershell
.\bins\mikro_hb.exe "C:\Users\Public\Documents\Mikroelektronika\mikroC PRO for PIC32\Examples\Internal MCU modules\P32MZ2048EFH144\ClickerMZ_Blinky\srcs\Clicker_Blinky.hex"
```

### Debug Mode (Hex Dump)
```powershell
.\bins\mikro_hb.exe --verbose "C:\Users\Public\Documents\Mikroelektronika\mikroC PRO for PIC32\Examples\Internal MCU modules\P32MZ2048EFH144\ClickerMZ_Blinky\srcs\Clicker_Blinky.hex"
```

---

## Reference Data

### Memory Map (PIC32MZ1048EFH064)
- **Program Flash**: 0x1D000000 - 0x1D0FFFFF (1MB)
- **Boot Flash**: 0x1D0F0000 - 0x1D0FFFFF (64KB, overlaps end of program)
- **Config Flash**: 0x1FC00000 - 0x1FC02FFF (12KB)

### Flash Specifications
- **Erase Block**: 16KB (0x4000)
- **Write Block**: 2KB (0x0800)
- **USB Packet**: 64 bytes
- **Packets per Write**: 2048 / 64 = 32 packets

### Device Info (from cmdINFO response)
```
38 01 15 00 08 00 00 00 00 00 10 00 03 00 00 40 04 00 00 08 05 00 00 12 ...
```
Parsed:
- Erase Block: 0x4000 (16KB) ✓
- Write Block: 0x0800 (2KB) ✓
- MCU Size: 0x080000 (512KB) ❌ WRONG - Should be 0x100000

---

## Critical Path Forward

**BLOCKER**: Must restore bootloader firmware before any further testing

**Then**:
1. Get exact PCAP byte data for comparison
2. Fix boot_line selection logic if needed
3. Verify all three memory regions match PCAP
4. Test with known-good hex file
5. Document working solution

---

## Notes

- Original code worked on **Linux** - now testing on **Windows**
- Cross-platform differences should be minimal (libusb-1.0)
- User confirmed original code was working, just had specific bugs
- **Do NOT rewrite** - fix bugs in original only
- HexFile_v2.c was a mistake - removed
