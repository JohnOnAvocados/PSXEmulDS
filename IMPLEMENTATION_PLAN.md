# PSXEmulDS Implementation Plan

## Project Overview

**Goal:** Create a PlayStation 1 emulator for Nintendo DS using software-only emulation.

**Reality Check:** Software-only emulation on the DS's 67MHz ARM9 is not sufficient to run PS1 games at playable framerates. The PS1 requires a MIPS R3000A CPU (~33MHz) plus GPU, SPU, and DMA emulation - too much for the DS to handle in software.

---

## Current Implementation Status

### Completed Components

| Component | File | Status |
|-----------|------|--------|
| CPU Core | `arm9/psx.c` | ~95% MIPS R3000A |
| Memory | `arm9/psx.c` | 1MB RAM |
| BIOS | `arm9/psx_bios.c` | Implemented |
| GPU | `arm9/psx_gpu.c` | Basic rendering |
| CD-ROM | `arm9/psx_cdrom.c` | Working |
| DMA | `arm9/psx_dma.c` | Working |
| GTE | `arm9/psx_gte.c` | Implemented |
| SIO | `arm9/psx_sio.c` | Implemented |
| PAD | `arm9/psx_pad.c` | Implemented |
| MDEC | `arm9/psx_mdec.c` | Implemented |
| SPU | `arm9/psx_spu.c` | Stub only |
| Slot-2 | `arm9/psx_slot2.c` | Disabled for safety |
| Memory Card | `arm9/psx_memctrl.c` | Implemented |

### Build Configuration

- **Optimization:** -O3 (changed from -O2)
- **Display:** MODE_5_2D with DMA copy
- **ARGV Support:** Enabled for TWiLight Menu++
- **Slot-2:** Manual detection only

---

## Hardware Investigation

### Why Software-Only Emulation Doesn't Work

| PS1 Component | Requirement | DS ARM9 (67MHz) |
|---------------|-------------|-----------------|
| CPU | ~33MHz MIPS | Too slow |
| GPU | Polygon rendering | No hardware assist |
| SPU | 24-channel ADPCM | No audio hardware |
| DMA | 7 channels | Software emulation |
| CD-ROM | ISO reading | Possible |

The DS simply lacks the processing power to emulate all PS1 components in real-time.

### Alternative Hardware Approaches Investigated

#### 1. Custom MIPS Cartridge (FPGA)

Attempted to create a custom cartridge with MIPS co-processor:

- **FPGA Required:** Xilinx Artix-7 100T (101k LUTs) - too large for DS cart
- **MIPS Cores:** MPX, aoR3000, PS-FPGA available
- **Issues:**
  - No FPGA fits 35mm cartridge form factor
  - PS1 needs ~4MB memory (can't fit on cart)
  - No video output path through cartridge
  - Power budget insufficient (~1.65W available)

#### 2. Existing Solutions

| Device | MIPS CPU? | Status | Price |
|--------|-----------|--------|-------|
| SuperCard DSTWO | Yes (JZ4732) | Discontinued | $80-150 |
| DSpico | No (RP2040) | Available, open-source | ~$50 |
| EZ-Flash Parallel | Has unused FPGA | Available | ~$40 |

**DSTWO** is the only option that works - it has a real MIPS processor that runs Linux with psx4all emulator (~10-15 FPS, no sound).

---

## Technical Details

### Memory Architecture

```
PS1 Memory Map (2MB RAM):
├── 0x00000000-0x001FFFFF : Expansion (unused)
├── 0x1F000000-0x1F7FFFFF : BIOS (512KB)
├── 0x1F800000-0x1F801FFF : I/O registers
├── 0x1F802000-0x1F803FFF : Scratchpad (2KB)
└── 0x20000000-0x203FFFFF : Main RAM (2MB)

DS Implementation:
- 1MB internal RAM (reduced from 2MB)
- 512KB BIOS
- 256x256 VRAM
- Total: ~1.65MB static
```

### DS Display

- **Mode:** MODE_5_2D (bitmap)
- **Resolution:** 192x192 (letterboxed from 256x240 PS1)
- **Frame copy:** DMA from VRAM to display buffer

### Slot-2 Implementation

- **Auto-detection:** Disabled (caused crashes on some carts)
- **Manual mode:** Press SELECT → A to detect
- **Supported:** SuperCard, M3, G6, EZ Flash detection code included

### TWiLight Menu++ Support

- ARGV parsing implemented
- Reads `.argv` file for game path
- Requires `.argv` file alongside `.nds`

---

## Development Notes

### Key Files

| File | Purpose |
|------|---------|
| `arm9/main.c` | DS entry, display, input |
| `arm9/psx.c` | CPU interpreter |
| `arm9/psx_gpu.c` | Graphics rendering |
| `arm9/psx_cdrom.c` | Disc reading |
| `arm9/psx_slot2.c` | RAM expansion |
| `include/psx_slot2.h` | Slot-2 definitions |

### Compiler Flags

```
-march=armv5te -mtune=arm946e-s -mthumb -O3
```

### Build Output

- `PSXEmulDS.nds` - Ready to run
- `PSXEmulDS.elf` - Debug symbols

---

## Future Directions

### If Resuming Development

1. **Fix SPU** - Implement actual sound output via ARM7
2. **Optimize CPU** - Lookahead JIT-style optimization
3. **Better display** - Faster VRAM copy
4. **Test on hardware** - Verify current state works

### Alternative Paths

1. **Move to 3DS** - Much faster, can run PS1 at full speed
2. **Use other handhelds** - RGB30/RG35XX have better emulation support
3. **Build custom hardware** - FPGA cartridge (significant effort)

---

## Research Sources

- GBATEK - DS/PS1 hardware reference
- PS-FPGA project - Verilog PS1 implementation
- pgate1/PlayStation_on_FPGA - Complete PS1 on FPGA
- DSpico - Open-source DS cartridge
- libnds/libfat documentation
- No$PSX specs

---

## Last Updated

2026-04-29

## Project History

- Created as educational PS1 emulator project
- Implemented full CPU/GPU/CDROM/DMA subsystems
- Found software-only emulation insufficient
- Investigated FPGA/MIPS cartridge alternatives
- Documented hardware approaches and limitations
- Project paused - hardware requirements not feasible for DS form factor