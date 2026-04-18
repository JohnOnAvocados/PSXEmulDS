# psxnds Implementation Plan

## Research Findings - 2026-04-18

### FAT/DLDI Issue Root Cause

**Problem**: FAT initialization fails on Ace3DS X flashcart via TwilightMenu++  
**Symptom**: `fatInit(0, 0)` returns false - cannot access SD card  
**Same as**: NesDS, GBARunner2 on same hardware  

**Root Cause Found**:
```c
// WRONG (our code):
boot->fat_ready = fatInit(0, 0);  

// CORRECT (working emulators):  
boot->fat_ready = fatInitDefault();  
```

**Why it fails**:
- `fatInit(0, 0)` = cache size 0 (no caching) + setAsDefaultDevice = false
- `fatInitDefault()` = default cache (5 pages = 20KB) + setAsDefaultDevice = true

The disk cache is required for any meaningful file access. Without it, I/O operations immediately fail.

### Implementation Steps

1. **FIX FAT**: Change fatInit(0, 0) -> fatInitDefault() ← CURRENT
2. **Rebuild & Test**: Verify SD card access works
3. **Continue**: BIOS loading, video output fixes

### References
- nds-hb-menu (devkitPro): Uses fatInitDefault()
- NesDS: Uses fatInitDefault()
- libfat docs: "-lfat must come before -lnds9" (we have this correct)

---

## Original Plan (Reference)

## Executive Summary

The emulator has a **functional CPU and basic memory system**, but lacks several critical features for actual game playback. Below is a prioritized roadmap.

---

## Phase A: Critical Fixes (Completed)

### A2: GPU Completion

**Status:** ✅ COMPLETED

| Implementation | Description |
|---------------|-------------|
| `gpu_fill_triangle()` | Filled polygon rendering using scanline algorithm |
| `gpu_fill_line()` | Filled line rendering |
| VRAM → VRAM DMA | GP0 0x40-0x4F transfers |
| CPU → VRAM DMA | GP0 0xA0 transfers |

### B1: CD-ROM Completion

**Status:** ✅ COMPLETED

| Implementation | Description |
|---------------|-------------|
| PAUSE command | Complete with response sequence |
| INIT command | With track info return |
| CUE parsing | Multi-track disc support |
| Track LBA | Track position array |

### B2: DMA Completion

**Status:** ✅ COMPLETED

| Implementation | Description |
|---------------|-------------|
| DMA channels | 0-3 channel support |
| Callbacks | GPU, CDROM, SPU callbacks |
| Interrupt handling | Completion detection |

---

## Phase B: CD-ROM and DMA (Completed)

### B1: Complete CD-ROM Command Handling

**Status:** ✅ DONE

| Command | Function | Status |
|---------|----------|---------|
| 0x02 | SETLOC (set read position) | ✅ Done |
| 0x06 | READN (start reading) | ✅ Done |
| 0x08 | STOP | ✅ Done |
| 0x09 | PAUSE | ✅ Done |
| 0x0A | INIT | ✅ Done |
| 0x0D | GETVOL | ✅ Partial |
| 0x0E | SETVOL | ✅ Partial |

### B2: Functional DMA Transfers

**Status:** ✅ DONE

| Channel | Use | Status |
|--------|-----|----------|
| DMA 0 | CD-ROM to RAM | ✅ Working |
| DMA 1 | GPU to RAM | ✅ Working |
| DMA 2 | RAM to GPU | ✅ Working |
| DMA 3 | SPU/Memory | ✅ Working |

---

## Phase C: Memory Expansion (Future)

### C1: SuperChis Prime RAM Access

**Technical Requirements:**

```
SuperChis Prime Specs:
- 32MB SDRAM (0x0C000000-0x0FFFFFFF region)
- 128KB FRAM (save games)
- 128MB NOR Flash (cartridge ROM)

Access Method:
- Detect via memory probe at known addresses
- Map SDRAM to ARM9 accessible region
- Create shared memory interface for PS1 RAM
```

| Task | Description | Complexity |
|------|-------------|------------|
| C1.1 | SuperChis detection routine | Medium |
| C1.2 | SDRAM initialization and testing | Medium |
| C1.3 | PS1 memory region mapping (0x00000000-0x007FFFFF) | High |
| C1.4 | Memory banking for >2MB access | High |

---

## Phase D: BIOS and System Calls

### D1: Complete BIOS Function Stubs

**Status:** Done (most common calls)

| Function | Address | Status |
|---------|---------|----------|
| Open | 0xA0 | ⚠️ Stub |
| Close | 0xA0 | ⚠️ Stub |
| Read | 0xA0 | ⚠️ Stub |
| Seek | 0xA0 | ⚠️ Stub |
| CdSeekL | 0xB0 | ✅ Working |
| CdRead | 0xB0 | ✅ Working |

---

## Phase E: Performance Optimization

### E1: Execution Speed

| Optimization | Description | Expected Gain |
|-------------|-------------|---------------|
| Batch instruction execution | Increase default batch size | 2-3x |
| Cached memory translation | Avoid address calculation per access | 10-20% |
| GPU render batching | Skip unchanged frames | Variable |

---

## Recommended Implementation Order

```
┌─────────────────────────────────────────────────────────────┐
│ COMPLETED:                                         │
│  A2 - GPU filled polygon rendering                    │
│  B1 - CD-ROM command completion                       │
│  B2 - DMA transfer implementation                 │
│  CUE - CUE sheet parsing                          │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│ FUTURE:                                           │
│  C1 - SuperChis RAM (restore full 2MB PS1 RAM)          │
│  E1 - Performance optimizations                  │
│  F1 - Sound implementation                       │
└─────────────────────────────────────────────────────────────┘
```

---

## Notes

- PS1 RAM currently 1MB (reduced from 2MB due to DS memory limits)
- GPU VRAM is 256x256 (reduced from 512x256)
- SuperChis Prime 32MB RAM could restore full 2MB PS1 RAM