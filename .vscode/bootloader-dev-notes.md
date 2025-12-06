# MikroC Bootloader Development Notes
**Last Updated: December 3, 2025**

## Project Structure

```
MikroC_bootloader/
├── bootloader/              # PIC32 firmware bootloader (mikroC)
│   ├── Makefile            # Root build config with trigger settings
│   ├── srcs/               # Bootloader source files
│   │   ├── Main.c          # Entry point, calls EnterBootloaderMode()
│   │   ├── UHB_Driver.c    # USB HID bootloader protocol
│   │   ├── Serial_Trigger.c # UART magic sequence detector (NEW)
│   │   ├── Serial_Trigger.h # UART trigger header (NEW)
│   │   ├── Config.c/h      # Device configuration
│   │   ├── USBdsc.c        # USB descriptors
│   │   └── Makefile        # Source-level build (needs manual file list)
│   ├── objs/               # Compiled .emcl objects
│   └── other/              # Generated .asm, .lst, .log files
├── srcs/                   # PC-side bootloader tool (Linux/Windows)
│   ├── MikroHB.c           # Main PC tool, sends magic sequence via serial
│   ├── Serial.c/h          # PC-side UART trigger sender
│   ├── USB.c               # libusb communication
│   └── Makefile
└── bins/
    └── mikro_hb.exe        # Windows PC tool (working)
```

## Current Status

### ✅ Completed
1. **PC-side tool (mikro_hb.exe)**
   - Ported to Windows 11 with MinGW
   - Serial trigger implemented (sends 0x55, 0xAA, 0x5B, 0xB5 via UART)
   - USB HID communication working
   - Command: `mikro_hb.exe --serial COM5 firmware.hex`

2. **Bootloader firmware structure**
   - Copied from `C:\Users\davec\source\mikroC_makefile_test`
   - Added Serial_Trigger.c/h files
   - Modified UHB_Driver.c to call `CheckSerialTrigger()`
   - Added trigger configuration to root Makefile

### ⚠️ In Progress - CRITICAL NEXT STEPS

#### 1. Fix Compilation (URGENT)
**Problem:** Serial_Trigger.c not being compiled

**Root Cause:** `srcs/Makefile` has hardcoded source file list at line 61:
```makefile
& "$(COMPILER_LOCATION)" ... "Main.c" "Config.c" "UHB_Driver.c" "USBdsc.c" ...
```

**Solution Required:**
- Manually add `"Serial_Trigger.c"` to the build command at line 61
- Line 44 already has `SOURCES := Main.c Config.c UHB_Driver.c USBdsc.c Serial_Trigger.c`
- But line 61 doesn't use `$(SOURCES)` variable - it's hardcoded
- **Quick Fix:** Add `"Serial_Trigger.c"` after `"USBdsc.c"` in line 61

#### 2. UART Configuration (CRITICAL)
**Problem:** UART pins and peripheral clock not configured

**Files to modify:**
- `bootloader/srcs/Config.c` - Add UART pin mapping in `Config()` function
- `bootloader/srcs/Serial_Trigger.c` - Verify U2BRG calculation for actual Fpb

**PIC32MZ2048EFH144 UART2 Default Pins:**
- RX: RG9 (pin 14)
- TX: RG8 (pin 12)

**Required Changes in Config.c:**
```c
void Config() {
    // Existing USB/flash config...
    
    // Configure UART2 pins for serial trigger
    // Set RG8 as output (TX), RG9 as input (RX)
    ANSELGCLR = 0x0300;  // Disable analog on RG8, RG9
    TRISGSET = 0x0200;   // RG9 as input (bit 9)
    TRISGCLR = 0x0100;   // RG8 as output (bit 8)
    
    // PPS (Peripheral Pin Select) if needed
    // U2RXR = 0b0001;   // Map U2RX to RPG9 (check datasheet)
    // RPG8R = 0b0010;   // Map RPG8 to U2TX (check datasheet)
}
```

**U2BRG Calculation:**
```c
// Current: U2BRG = 433 assumes 200MHz peripheral clock
// Formula: U2BRG = (Fpb / (4 * baud)) - 1
// For 115200 baud @ 200MHz: (200000000 / 460800) - 1 = 433 ✓
// Verify actual Fpb from oscillator config!
```

#### 3. Trigger Mode Configuration
**Added to root Makefile (lines 76-82):**
```makefile
ENABLE_USB_TRIGGER    := 1    # Wait for USB boot command
ENABLE_SERIAL_TRIGGER := 1    # Check UART for magic sequence
ENABLE_BOTH_TRIGGERS  := 1    # Require BOTH (USB AND Serial)
SERIAL_UART_MODULE    := 2    # UART module number
SERIAL_BAUD_RATE      := 115200
SERIAL_TIMEOUT_MS     := 50
```

**TODO:** Pass these to compiler as #defines
```makefile
# Add to CFLAGS in root Makefile:
TRIGGER_DEFINES := -DENABLE_USB_TRIGGER=$(ENABLE_USB_TRIGGER) \
                   -DENABLE_SERIAL_TRIGGER=$(ENABLE_SERIAL_TRIGGER) \
                   -DENABLE_BOTH_TRIGGERS=$(ENABLE_BOTH_TRIGGERS) \
                   -DSERIAL_UART_MODULE=$(SERIAL_UART_MODULE) \
                   -DSERIAL_BAUD_RATE=$(SERIAL_BAUD_RATE) \
                   -DSERIAL_TIMEOUT_MS=$(SERIAL_TIMEOUT_MS)

CFLAGS := $(CFLAGS_COMMON) -pP$(DEVICE) ... $(TRIGGER_DEFINES)
```

**Update UHB_Driver.c EnterBootloaderMode():**
```c
char EnterBootloaderMode() {
    char dataRx;
    unsigned long timer = 800000;

    #if ENABLE_SERIAL_TRIGGER == 1
    // Check for UART serial trigger first
    if (CheckSerialTrigger()) {
        #if ENABLE_BOTH_TRIGGERS == 1
        // If both required, continue to USB check
        // (fall through to USB polling loop)
        #else
        // Serial only - enter bootloader immediately
        return 1;
        #endif
    }
    #endif

    #if ENABLE_USB_TRIGGER == 1
    // Check for USB bootloader request loop
    while (1) {
        USB_Polling_Proc();
        dataRx = HID_Read();
        // ... existing USB check code ...
    }
    #else
    // USB disabled - timeout immediately if serial didn't trigger
    return 0;
    #endif
}
```

## Build Process

### Current Build Command
```powershell
cd C:\Users\davec\GIT\MikroC_bootloader\bootloader
make clean
make
```

### Build Output
- Binary: `srcs/USB HID Bootloader.hex`
- Objects: `objs/*.emcl`
- Assembly: `other/*.asm` (if enabled)

### Compiler
- **mikroC PRO for PIC32** (command-line)
- Path: `C:\Users\Public\Documents\Mikroelektronika\mikroC PRO for PIC32\mikroCPIC32.exe`

## Bootloader Flow

### Current Implementation
1. `main()` → unlocks boot flash, calls `Config()`, enables USB
2. Calls `EnterBootloaderMode()`
3. **NEW:** `CheckSerialTrigger()` runs first (checks UART for magic bytes)
4. If no serial trigger → waits 5 seconds for USB `cmdBOOT` command
5. If timeout → `StartProgram()` (run application)
6. If triggered → `StartBootloader()` (flash mode)

### Magic Sequence
- **Bytes:** `0x55, 0xAA, 0x5B, 0xB5`
- **Defined in:** 
  - PC tool: `srcs/Serial.c` (sender)
  - Firmware: `bootloader/srcs/Serial_Trigger.h` (receiver)

## VS Code Extension Plan

### Phase 1: Planning (TONIGHT)
- Define extension features
- Choose implementation approach:
  - **Option A:** Wrap mikro_hb.exe (simple, fast)
  - **Option B:** Node.js USB library (complex, integrated)
  - **Option C:** Hybrid (exe for flash, JS for detection)

### Phase 2: Extension Features
- ⚡ Status bar button with PIC32 device detection
- 📁 Hex file picker from workspace
- 🔌 COM port dropdown with auto-detection
- ⚙️ Settings: baud rate, trigger mode, paths
- 📊 Output panel with real-time progress
- 🎯 Commands: Flash, Detect Device, Clear Flash

### Phase 3: Future Repo
- Move `bootloader/` to separate Git repo after working
- Keep `mikro_hb.exe` tool in this repo
- VS Code extension in its own repo (publishable)

## Key Files Reference

### PC Tool
- `srcs/MikroHB.c:88` - Calls `serial_trigger_bootloader()`
- `srcs/Serial.c:57` - Sends magic sequence
- `srcs/USB.c:33` - HID interrupt transfers

### Bootloader Firmware
- `bootloader/srcs/Main.c:88` - Entry point
- `bootloader/srcs/UHB_Driver.c:452` - `EnterBootloaderMode()` **← MODIFIED**
- `bootloader/srcs/UHB_Driver.c:94` - Added `#include "Serial_Trigger.h"`
- `bootloader/srcs/Serial_Trigger.c:19` - `CheckSerialTrigger()` implementation
- `bootloader/srcs/Config.c` - **NEEDS UART PIN CONFIG**

## Immediate Action Items for Tonight

1. ✅ **Fix compilation** - add Serial_Trigger.c to build command
2. ✅ **Configure UART pins** - modify Config.c
3. ✅ **Pass Makefile defines** - add TRIGGER_DEFINES to compiler flags
4. ✅ **Update EnterBootloaderMode()** - add conditional compilation
5. ✅ **Test build** - verify no errors
6. ⏳ **Plan VS Code extension** - decide architecture
7. ⏳ **Test with hardware** - if PIC32 available

## Testing Checklist

### Without Hardware
- [ ] Bootloader compiles successfully
- [ ] Binary size reasonable (<50KB)
- [ ] No compiler warnings about UART registers

### With Hardware
- [ ] UART trigger works (receive magic sequence → enter bootloader)
- [ ] USB trigger works (receive cmdBOOT → enter bootloader)
- [ ] Timeout works (no trigger → run application)
- [ ] Flash programming works via USB HID
- [ ] Application boots after successful flash

## Notes & Decisions

### mikroC Compiler Quirks
- Uses `.emcl` format (mikroElektronika Compiled Library)
- Assembly output only meaningful if `-DL` flag removed
- Library files must be in search path (`-SP` and `-IP` flags)
- `.mcp32` project file required even for CLI builds

### UART Peripheral Clock
- **CRITICAL:** Verify Fpb (peripheral bus clock frequency)
- PIC32MZ can have different Fpb than Fosc
- Check `Config.c` oscillator/PLL configuration
- U2BRG must match actual clock or baud rate will be wrong

### Design Decision: Trigger Priority
- Serial check runs FIRST (fast, ~50ms timeout)
- Then USB polling (5 seconds timeout)
- Rationale: Serial is quicker to detect, USB needs enumeration time

## Questions for User

1. What UART pins are accessible on your PCB?
2. What is the actual peripheral clock (Fpb) configured in Config.c?
3. Do you want serial-only, USB-only, or both trigger modes?
4. For VS Code extension: wrap exe or reimplement in Node.js?

---
**Next Session:** Continue with UART configuration, test build, plan VS Code extension architecture.
