# PSXEmulDS Implementation Plan

PlayStation 1 emulator for Nintendo DS - consolidated implementation roadmap.

## Current Status (Alpha)

The emulator has a functional CPU core and memory system, plus all major PS1 subsystems implemented:
- CPU, Memory, BIOS, Interrupts, Timers - COMPLETE
- GPU, CD-ROM, DMA - COMPLETE
- GTE, SIO, PAD, MDEC - COMPLETE
- Sound (SPU) - STUB (silence only)

## Completed Phases

### Phase 1-3: Core System
- MIPS R3000A CPU (~95% coverage)
- Memory system (1MB RAM, 512KB BIOS)
- Interrupt system, timers
- BIOS stubs

### Phase 4-6: Storage & Graphics
- CD-ROM controller
- DMA controller
- GPU with VRAM

### Phase 7-8: Peripherals
- Controller (PAD) polling
- Memory card support
- MDEC video decoder

### Phase 9: GTE & System
- Geometry Transformation Engine
- Serial Interface (SIO)
- Memory Control registers

## Next Phases (Priority Order)

### Phase 10: Sound (SPU) - HIGH PRIORITY
| Component | Implementation | Priority |
|-----------|---------------|-----------|
| ADPCM decoder | 24-channel ADPCM | High |
| ADSR envelopes | Attack/Decay/Sustain/Release | High |
| Modular design | Can compile out when disabled | HIGH |

**Design**: Compile-time option to disable for performance

### Phase 11: Display Output - HIGH PRIORITY
| Component | Target |
|-----------|--------|
| Resolution | 192x192 (DS screen optimized) |
| Frame copy | Optimized VRAM→DS |
| V-blank | Sync |
| Interlace | Field toggling |

### Phase 12: Controller Input - HIGH PRIORITY
| Component | Implementation |
|-----------|------------|
| PAD polling | Serial read every frame |
| Button map | DS → PS1 mapping |
| Analog | DualShock ready |

### Phase 13: Game Loading - MEDIUM
| Component | Implementation |
|-----------|------------|
| CUE parser | Track LBA mapping |
| ISO9660 | Sector reading |
| Multi-bin | File switching |

### Phase 14: Memory Expansion - MEDIUM
| Component | Implementation |
|-----------|------------|
| Slot-2 RAM | 2MB PS1 RAM |
| Banking | 2MB→32MB mapping |

## Architecture Notes

### Memory Budget (DS 3.5MB ARM9 limit)
| Component | Size |
|-----------|------|
| PS1 RAM | 1 MB |
| PS1 BIOS | 512 KB |
| GPU VRAM | 256×256 |
| Scratchpad | 1 KB |
| I/O | 4 KB |
| **Total Static** | ~1.65 MB |

### Display Configuration
- Target: 192×192 (fits DS screen)
- PS1 original: 256×240
- Scale: 0.75x with letterboxing

### Performance Targets
- PS1 CPU: 33.8688 MHz MIPS R3000A
- DS ARM9: 67 MHz
- Expected: 2-5 FPS (simple games), <1 FPS (complex)

## Controls (In-Emulator)

| Button | Action |
|--------|--------|
| A | Execute 1 instruction |
| B | Execute current batch |
| Y | Execute 8 batches |
| X | Toggle auto-run |
| L | Halve batch size |
| R | Double batch size |
| START | Reload/reboot |
| SELECT | Toggle test mode |

## File Loading (Priority)

1. `/psx/boot.exe` or `/PSX/BOOT.EXE`
2. `/psx/demo.bin` (raw at 0x10000)
3. `/psx/scph1001.bin` (BIOS boot)
4. Built-in demo

## Build

```powershell
make
```

Produces `PSXEmulDS.nds`.

## Technical References

- psx-spx.consoledev.net - Full PS1 hardware specs
- devkitPro / libnds - DS homebrew SDK
- libfat - SD card access

## Known Limitations

- Sound: Stub only (no audio output)
- Display: Basic VRAM copy
- Controller: Not yet mapped to DS input
- Memory: 1MB (2MB via Slot-2)

## Future Development

1. Sound implementation (Phase 10)
2. Display optimization (Phase 11)
3. Controller integration (Phase 12)
4. Game loading (Phase 13)
5. Memory expansion (Phase 14)

---
Last Updated: 2026-04-19