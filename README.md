# psxnds

`psxnds` is a Nintendo DS homebrew proof of concept for experimenting with a tiny PlayStation 1 emulator on real DS hardware.

The goal is to create a PS1 emulator that boots games and runs them at lower FPS rather than constantly crashing. While not expected to match PC emulator performance, functional game execution is the priority.

## Current Status - Phase 1 Complete

The project has completed Phase 1 (Stabilization):

- Complete MIPS R3000A instruction set (~95% coverage)
- Interrupt system with hardware IRQ support
- Timer hardware (Timer 0-2)
- Expanded BIOS call stubs
- Modular sound stub (branch-ready)
- Slot-2 RAM support (SuperCard compatible)
- Batch testing mode with error codes

## Build

You need a working `devkitPro` + `libnds` environment.

```powershell
make
```

This should produce `psxnds.nds`.

## Project Layout

- `source/main.c`: DS entry point and on-screen debugger
- `source/psx.c`: PS1 core logic and CPU interpreter
- `source/psx_exe.c`: PS-X EXE parsing and RAM loading
- `source/psx_spu.c`: Sound stub (modular, can be disabled)
- `source/psx_slot2.c`: Slot-2 RAM expansion support
- `include/psx.h`: Core state definitions and interrupt constants
- `include/psx_exe.h`: Executable loader definitions
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

### Phase 2: Boot Capability (IN PROGRESS)
Goal: Games can load from CD and reach main menu

- CD-ROM controller implementation
- DMA controller implementation
- GPU/VDC skeleton
- Memory control registers

### Phase 3: Runtime (PLANNED)
Goal: Games run and display, even if slowly

- Complete GPU/graphics subsystem
- Proper timing and interrupt timing
- Software framebuffer rendering

### Phase 4: Performance & Polish (PLANNED)
Goal: Better FPS and user experience

- Performance optimization
- Game selection menu
- Save state support

## Controls

- `A`: execute 1 instruction
- `B`: execute the current batch size
- `Y`: execute 8 batches at once
- `X`: toggle auto-run
- `L` / `R`: halve or double the batch size
- `START`: reload files and reboot emulator state
- `SELECT`: toggle test mode
- `Z`: save test results to SD card

## Testing Procedures

To reduce SD card wear, use the batch testing system:

1. Press `SELECT` to enable test mode
2. Use `A`, `B`, or `Y` to run instructions in batches
3. Each batch run records a test result
4. Error codes are displayed on screen
5. Press `Z` to save all results to `/psx/test_results.txt`

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

The emulator supports Slot-2 RAM expansions (tested with SuperCard):

- Automatically detects Slot-2 RAM at boot
- Can map additional PS1 RAM if available
- Currently supports up to 16MB Slot-2 buffer

## Memory Architecture

- DS has 4MB main RAM + 656KB video RAM
- PS1 games expect 2MB RAM (standard) or 8MB (expansion)
- PS1 address space: 0x00000000-0x007FFFFF maps to DS RAM
- Framebuffer will consume significant RAM in Phase 3

## Performance Targets

- PS1 runs at 33MHz MIPS R3000A
- DS ARM9 runs at 67MHz
- Expected: 2-5 FPS for simple games, <1 FPS for complex games
- Frame skipping will be necessary

## Architecture Notes

### Sound Subsystem

The sound subsystem (`psx_spu.c`, `psx_spu.h`) is modular and designed for easy disablement:

- Can be compiled out by removing from Makefile
- Preserves original functionality in separate branch
- Minimal implementation - outputs silence
- Ready for full SPU implementation in future

### RAM Backend Abstraction

The emulator keeps RAM access behind a backend abstraction:
- Current builds use internal DS RAM
- Slot-2 RAM can be used for expanded PS1 memory
- Future: different backing stores possible without CPU core rewrites

## Homebrew Notes

This project targets standard Nintendo DS homebrew through `libnds` and `libfat`. You do not need a custom DSTWO-style plugin runtime.

## Known Limitations

- No CD-ROM emulation yet (Phase 2)
- No GPU rendering yet (Phase 3)
- No sound output yet (modular stub only)
- Performance will be very low
- Only simple games may boot initially
