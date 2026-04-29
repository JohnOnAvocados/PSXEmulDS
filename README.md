# PSXEmulDS

A Nintendo DS homebrew PlayStation 1 (PS1) emulator.

## Overview

PSXEmulDS is a software-based PS1 emulator targeting the original Nintendo DS and DS Lite hardware. It aims to emulate the Sony PlayStation 1 using only the DS's ARM9 processor without requiring any special cartridge hardware.

**Important:** This project is primarily an educational/learned project. Due to the DS's limited processing power (67MHz ARM9 vs the PS1's 33MHz MIPS R3000A), full-speed game playback is not achievable with software-only emulation. Actual game execution requires specialized hardware like the discontinued SuperCard DSTWO (which has its own MIPS co-processor).

## Current Status

| Component | Status |
|-----------|--------|
| CPU (MIPS R3000A) | Implemented (~95%) |
| Memory (1MB RAM) | Implemented |
| BIOS | Implemented (stub) |
| GPU/Graphics | Implemented (basic) |
| CD-ROM | Implemented |
| DMA | Implemented |
| GTE (Geometry) | Implemented |
| SPU (Sound) | Stub only |
| Controller | Implemented |
| Memory Card | Implemented |

## Requirements

- **Hardware:** Nintendo DS, DS Lite, or DSi (original DS preferred for homebrew)
- **Software:** devkitPro with libnds and libfat
- **Storage:** MicroSD card for loading PS1 games

## Building

```bash
make
```

This produces `PSXEmulDS.nds` which can be run on the DS via:
- TWiLight Menu++ (recommended)
- Direct boot via flashcart
- GBARunner2 (for Slot-1)

## Project Structure

```
PSXEmulDS/
├── arm9/              # ARM9 (main) source code
│   ├── main.c         # DS entry point
│   ├── psx.c          # PS1 CPU core
│   ├── psx_gpu.c      # Graphics processing
│   ├── psx_cdrom.c    # CD-ROM controller
│   ├── psx_dma.c      # DMA controller
│   ├── psx_spu.c      # Sound (stub)
│   └── ...
├── arm7/              # ARM7 (audio/input) source
├── include/           # Header files
├── icon.bmp           # NDS icon
├── Makefile           # Build configuration
└── README.md          # This file
```

## Running Games

### Via TWiLight Menu++

1. Copy `PSXEmulDS.nds` to your SD card
2. Place PS1 game files in `/psx/` folder
3. Launch via TWiLight Menu++

### Supported File Formats

- `.exe` - PS-X executable files
- `.bin` - Raw binary files
- `.iso` - CD disc images (basic support)

### File Loading Priority

1. `/psx/boot.exe` or `/PSX/BOOT.EXE`
2. `/psx/demo.bin` (raw binary at 0x00010000)
3. `/psx/scph1001.bin` (BIOS boot)
4. Built-in demo

## Controls

| Button | Action |
|--------|--------|
| D-Pad | Move |
| A | Confirm |
| B | Cancel |
| START | Start |
| SELECT | Open menu |

## Configuration

The emulator creates `psxemu.ini` on first run for storing settings.

## Known Limitations

- **Performance:** Very low FPS (1-5 FPS typical) - software-only emulation is too slow
- **Sound:** No audio output (SPU stub)
- **Compatibility:** Limited - only simplest games may boot
- **Memory:** 1MB PS1 RAM (reduced from original 2MB)

## Hardware Notes

### Why Software-Only Emulation Is Difficult

The Nintendo DS's ARM9 processor (67MHz) is significantly slower than what's needed to emulate a PS1 (33MHz MIPS + GPU + SPU). The PS1 requires:

- ~33 MHz MIPS R3000A CPU emulation
- GPU polygon rendering
- ADPCM audio decoding
- CD-ROM data streaming
- DMA transfers

Even with aggressive optimization, software-only emulation cannot achieve playable framerates.

### Working Alternatives

For actual PS1 gameplay on DS hardware:

1. **SuperCard DSTWO** (discontinued, ~$80-150)
   - Has built-in MIPS co-processor
   - Runs psx4all emulator
   - Achieves ~10-15 FPS, no sound

2. **Nintendo 3DS** (New model)
   - Much faster ARM11 processor
   - Can run PS1 emulators at full speed

3. **Other Handhelds**
   - RGB30, RG35XX, Miyoo Mini
   - Run PS1 emulation well via Rockchip/RTL SoCs

## Development

### Adding Sound

The SPU is currently a stub. To implement:
1. Modify `arm9/psx_spu.c`
2. Use ARM7 for audio output via IPC
3. Enable libmad or similar for ADPCM

### Performance Optimization

Current flags in Makefile:
- `-O3` optimization enabled
- `-march=armv5te` - ARM946E-S target

### Slot-2 Support

Auto-detection is disabled for safety. Manual detection available:
1. Press SELECT to enter manual mode
2. Press A to detect Slot-2 RAM

## License

This project is for educational purposes. PS1 BIOS and game files are copyright their respective owners.

## References

- [GBATEK](https://problemkaputt.de/gbatek.htm) - DS/PS1 hardware documentation
- [No$PSX](https://problemkaputt.de/psx.htm) - PS1 hardware specs
- [devkitPro](https://devkitpro.org/) - DS homebrew SDK
- [libfat](https://github.com/nickelpack/libfat) - SD card access

## Contact

For issues/updates, check the project repository.