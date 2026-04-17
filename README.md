# psxnds

`psxnds` is a Nintendo DS homebrew proof of concept for experimenting with a tiny PlayStation 1 emulator on real DS hardware.

The goal is to create a PS1 emulator that boots games and runs them at lower FPS rather than constantly crashing. While not expected to match PC emulator performance, functional game execution is the priority.

## Current Status - Phase 2-3 Complete

The project has completed Phase 1 (Stabilization), Phase 2 (Boot Capability), and Phase 3 (Runtime):

**Phase 1 - Stabilization:**
- Complete MIPS R3000A instruction set (~95% coverage)
- Interrupt system with hardware IRQ support
- Timer hardware (Timer 0-2)
- Expanded BIOS call stubs
- Modular sound stub (branch-ready)
- Slot-2 RAM support (SuperCard compatible)
- Batch testing mode with error codes

**Phase 2 - Boot Capability:**
- CD-ROM controller implementation
- DMA controller implementation
- PS-X EXE parsing and loading

**Phase 3 - Runtime:**
- GPU/VDC skeleton with VRAM
- Software framebuffer rendering
- V-blank interrupt timing
- Peripheral update system

## Build

You need a working `devkitPro` + `libnds` environment.

```powershell
make
```

This should produce `psxnds.nds`.

## Project Layout

**Core Files:**
- `source/main.c`: DS entry point and on-screen debugger
- `source/psx.c`: PS1 core logic and CPU interpreter
- `include/psx.h`: Core state definitions and interrupt constants

**Subsystem Files:**
- `source/psx_exe.c`: PS-X EXE parsing and RAM loading
- `source/psx_cdrom.c`: CD-ROM controller implementation
- `source/psx_dma.c`: DMA controller implementation
- `source/psx_gpu.c`: GPU/VDC with VRAM (256x256)
- `source/psx_spu.c`: Sound stub (modular, can be disabled)
- `source/psx_slot2.c`: Slot-2 RAM expansion support

**Header Files:**
- `include/psx_exe.h`: Executable loader definitions
- `include/psx_cdrom.h`: CD-ROM register definitions
- `include/psx_dma.h`: DMA channel definitions
- `include/psx_gpu.h`: GPU register definitions
- `include/psx_spu.h`: Sound register definitions
- `include/psx_slot2.h`: Slot-2 device definitions

## Implementation Roadmap

### Phase 1: Stabilization (COMPLETE)
Goal: Games don't immediately halt on unimplemented opcodes

- Complete MIPS R3000A instruction set
- Implement interrupt system (COP0, hardware interrupts)
- Implement timer hardware (Timer 0-2)
- Expand BIOS call stubs
- Add error code system for testing

### Phase 2: Boot Capability (COMPLETE)
Goal: Games can load from CD and reach main menu

- CD-ROM controller implementation
- DMA controller implementation
- GPU/VDC skeleton
- Memory control registers

### Phase 3: Runtime (COMPLETE)
Goal: Games run and display, even if slowly

- Complete GPU/graphics subsystem
- Proper timing and interrupt timing
- Software framebuffer rendering

### Phase 4: Performance & Polish (PLANNED)
Goal: Better FPS and user experience

- Performance optimization
- Game selection menu
- Save state support
- Slot-2 RAM integration (SuperChis Prime 32MB SDRAM)

## Controls

- `A`: execute 1 instruction
- `B`: execute the current batch size
- `Y`: execute 8 batches at once
- `X`: toggle auto-run
- `L`: halve batch size OR save test results (in test mode)
- `R`: double the batch size
- `START`: reload files and reboot (also saves test results)
- `SELECT`: toggle test mode (also saves results when exiting)

## Testing Procedures

To reduce SD card wear, use the batch testing system:

1. Press `SELECT` to enable test mode
2. Use `A`, `B`, or `Y` to run instructions in batches
3. Each batch run records a test result
4. Error codes are displayed on screen
5. Test results auto-save when pressing `SELECT` to exit test mode or `START` to reload

### Error Codes

- `0x0001`: General halt (unsupported opcode)
- `0x0002`: Syscall
- `0x0003`: Break instruction
- `0x0004`: Unimplemented COP0 function
- `0xFFFF`: Already halted

### Test Results Format

When saved to SD card, test results include:
- Total tests passed/failed
- Per-test error codes
- Cycle count at error
- PC and opcode at error
- Error description

## File Loading

At startup, the app tries to load files in this order:

1. `/psx/demo.exe` or `/psx/boot.exe` or `/PSX/BOOT.EXE`
2. `/psx/demo.bin` (raw binary at 0x00010000)
3. `/psx/scph1001.bin` or `/psx/bios.bin` (BIOS boot)
4. Built-in demo

## BIOS Mode

If a legal BIOS image is present, the emulator starts at the PS1 BIOS reset vector (0xBFC00000).

This is a better proof-of-concept target than raw binaries because:
- It proves BIOS mapping works
- It gives real instruction traces
- It shows which opcodes and hardware areas need implementation

## Slot-2 RAM Support

The emulator supports Slot-2 RAM expansions (tested with SuperCard and SuperChis Prime):

- Automatically detects Slot-2 RAM at boot
- Can map additional PS1 RAM if available
- Currently supports up to 16MB Slot-2 buffer
- Future: Full 32MB SDRAM support via SuperChis Prime

## Memory Architecture

The emulator uses reduced memory sizes to fit within the DS 3.5MB ARM9 limit:

| Component | Size | Location |
|-----------|------|----------|
| PS1 RAM | 1 MB | Internal (reduced from 2MB) |
| PS1 BIOS | 512 KB | Internal |
| GPU VRAM | 256x256 | Internal |
| Scratchpad | 1 KB | Internal |
| I/O Registers | 4 KB | Internal |
| **Total Static** | ~1.65 MB | |

**Headroom:** ~1.85 MB available for future features or Slot-2 integration.

Note: Full 2MB PS1 RAM and larger VRAM can be enabled via Slot-2 RAM when using expansion cartridges.

## Hardware Requirements

- Nintendo DS Lite or original DS (tested with NDSL)
- Slot-2 flashcart (SuperCard or SuperChis Prime recommended)
- MicroSD card for game files

### Recommended Setup

For best compatibility:
1. Use SuperChis Prime in Slot-2 (128MB NOR + 32MB SDRAM)
2. Load via TwilightMenu++ or direct SuperFW
3. Future: Direct Slot-2 RAM integration for full PS1 memory

## Performance Targets

- PS1 runs at 33MHz MIPS R3000A
- DS ARM9 runs at 67MHz
- Expected: 2-5 FPS for simple games, <1 FPS for complex games
- Frame skipping will be necessary

## Architecture Notes

### Modular Subsystem Design

Each PS1 subsystem is implemented as a separate module:

```
psx_cdrom.c ← CD-ROM controller (Phase 2)
psx_dma.c   ← DMA channels (Phase 2)
psx_gpu.c   ← GPU/VDC (Phase 3)
psx_spu.c   ← Sound stub (Phase 1)
psx_slot2.c ← Slot-2 RAM (Phase 1)
```

All subsystems are initialized via `psx_init_*()` functions and updated via `psx_update_peripherals()`.

### RAM Backend Abstraction

The emulator keeps RAM access behind a backend abstraction:
- Current builds use internal DS RAM (1MB for PS1)
- Slot-2 RAM can be used for expanded PS1 memory
- Future: different backing stores possible without CPU core rewrites

### Sound Subsystem

The sound subsystem (`psx_spu.c`, `psx_spu.h`) is modular and designed for easy disablement:

- Can be compiled out by removing from Makefile
- Preserves original functionality in separate branch
- Minimal implementation - outputs silence
- Ready for full SPU implementation in future

## Homebrew Notes

This project targets standard Nintendo DS homebrew through `libnds` and `libfat`. You do not need a custom DSTWO-style plugin runtime.

## Known Limitations

- No CD-ROM disc reading (ISO loading in progress)
- Performance will be very low
- Only simple games may boot initially
- 1MB PS1 RAM may limit some games (Slot-2 expansion planned)
- Sound is stub-only

## Future Development

See the implementation roadmap above. Key areas for improvement:

1. **Slot-2 RAM Integration:** Use SuperChis Prime 32MB SDRAM for full 2MB PS1 RAM
2. **ISO/CD Loading:** Parse and load actual PS1 disc images
3. **SPU Implementation:** Replace stub with functional sound output
4. **GPU Completion:** Full polygon rendering and texture support
5. **Timing Refinement:** Accurate PS1 bus timing emulation
