# PSX Emulator Detailed Implementation Plan

Based on analysis of https://psx-spx.consoledev.net/ specifications, this document outlines the remaining work needed to achieve accurate PS1 emulation.

## Current Status
The emulator has been fixed to:
- Properly initialize FAT using fatInitDefault()
- Load BIOS from SD card using multiple path scanning
- Compile without errors/warnings
- Basic memory map constants defined

## Remaining Work Phases

### Phase 1: Core Memory System & CPU Execution (Weeks 1-2)

#### 1.1 Memory Map Enhancement
- [ ] Implement proper address translation with KUSEG/KSEG0/KSEG1/KSEG2 regions
- [ ] Add memory mirroring:
  - 2MB RAM mirrored to first 8MB (enabled by default)
  - 512K BIOS ROM mirrored to last 4MB (disabled by default)
- [ ] Implement write queue emulation (4-word deep pass-through)
- [ ] Ensure KSEG1 is used for hardware registers to avoid write queue issues
- [ ] Add i-Cache implementation (4096 bytes, direct-mapped, 256 lines)
- [ ] Ensure scratchpad (1F800000h-1F8003FFh) is non-executable

#### 1.2 CPU Execution Improvements
- [ ] Implement proper exception handling:
  - Memory error exceptions (misalignments)
  - Bus error exceptions (unused memory regions)
- [ ] Complete COP0 register implementation:
  - Status register (IM, KUo/IEo, KUp/IEr, KUc/IEc)
  - Cause register (IP, ExcCode)
  - EPC (Exception Program Counter)
  - PrId (Processor Revision Identifier)
  - Config, LLAddr, WatchLo/Hi, Context, BadVAddr
- [ ] Implement delay slot handling for branch/jump instructions
- [ ] Add proper system call (syscall) and break (break) instruction handling
- [ ] Implement cache operations if needed (though PSX has limited cache control)

#### 1.3 Memory Protection and Access
- [ ] Implement proper memory access protections:
  - User mode (KUSEG) vs kernel mode (KSEG0/KSEG1) restrictions
  - Unused memory regions causing bus errors
  - Memory alignment requirements
- [ ] Add proper handling of memory mirrors and their attributes

### Phase 2: BIOS Subsystem Implementation (Weeks 3-4)

#### 2.1 BIOS Memory Initialization
- [ ] Reserve first 64K of RAM for BIOS/kernel usage
- [ ] Initialize BIOS-specific memory areas:
  - System stack area
  - Exception vectors (0x80000080)
  - Bootmem allocation structures
  - Various fixed address data structures in first 500h bytes

#### 2.2 Core BIOS Functions
- [ ] Implement essential file operations:
  - open(filename, accessmode)
  - read(fd, dst, length)
  - write(fd, src, length)
  - close(fd)
  - lseek(fd, offset, seektype)
  - getc(fd) / putc(char,fd)
- [ ] Implement memory management:
  - malloc(size) / free(buf)
  - calloc(sizx, sizy)
  - realloc(old_buf, new_size)
  - InitHeap(addr, size)
- [ ] Implement string and memory functions:
  - memcpy, memset, memmove, memcmp, memchr
  - strcpy, strncpy, strlen, strcmp, strncmp
  - bcopy, bzero, bcmp
- [ ] Implement number conversion:
  - atoi, atol, atob, strtol, strtoul
  - abs, labs
  - Note: Avoid COP1 FPU functions as they're not present in original PSX

#### 2.3 BIOS Execution Functions
- [ ] Implement executable loading:
  - LoadTest(filename, headerbuf)
  - Load(filename, headerbuf)
  - Exec(headerbuf, param1, param2)
  - LoadExec(filename, stackbase, stackoffset)
  - FlushCache()
- [ ] Implement configuration functions:
  - SetConf(num_EvCB, num_TCB, stacktop)
  - GetConf(num_EvCB_dst, num_TCB_dst, stacktop_dst)

#### 2.4 BIOS Boot Process
- [ ] Implement proper BIOS entry point execution (0xBFC00000)
- [ ] Add hardware initialization sequence:
  - Initialize cop0 registers
  - Set up exception vectors
  - Initialize hardware timers
  - Set up interrupt controller
- [ ] Implement boot menu detection and execution
- [ ] Add proper handling of SystemError and other BIOS error conditions

### Phase 3: Event & Interrupt System (Weeks 5-6)

#### 3.1 Interrupt Controller
- [ ] Implement MIPS interrupt system:
  - Status register (IEc, IEC, IM bits)
  - Cause register (IP hardware interrupt flags, ExcCode)
  - Proper interrupt masking and priority handling
- [ ] Add hardware interrupt sources with correct mapping:
  - VBlank (GPU)
  - GPU (command FIFO ready)
  - CDROM (command/data ready)
  - DMA (channel completion)
  - Timer0, Timer1, Timer2
  - SPU (sound buffer ready)
  - SIO (serial I/O)
- [ ] Implement interrupt latency and handling timing

#### 3.2 Event System
- [ ] Implement Event Control Blocks (EvCB) - 16 blocks of 28 bytes each
- [ ] Implement Timer/Vblank events
- [ ] Implement CDROM events (command completion, data ready, etc.)
- [ ] Implement Memory Card events
- [ ] Implement proper event enabling/disabling
- [ ] Implement event waiting and testing functions
- [ ] Implement event delivery and callback mechanism

### Phase 4: Timers & DMA System (Weeks 7-8)

#### 4.1 Timer Implementation
- [ ] Implement 3 timers with:
  - 24-bit counters (with 32-bit mode option)
  - Reload capability
  - IRQ generation on match
  - Various clock divisors
  - Gate and reset controls
- [ ] Add accurate timing synchronization with CPU clock (~33.8688MHz)
- [ ] Implement timer IRQ triggering with proper priority

#### 4.2 DMA Channels
- [ ] Implement all 7 DMA channels with correct specifications:
  - MDEC-in (channel 0)
  - MDEC-out (channel 1)
  - GPU (channel 2)
  - CDROM (channel 3)
  - SPU (channel 4)
  - PIO (channel 5)
  - OTC (channel 6)
- [ ] Add channel linking and synchronization capabilities
- [ ] Implement DMA-triggered IRQs with correct timing
- [ ] Add correct behavior for each channel type:
  - GPU: VRAM transfers, command fetching
  - CDROM: sector data transfer, command/response
  - SPU: audio sample transfer, reverb work area
  - OTC: order table chain for GPU drawing
  - PIO: expansion port communication
  - MDEC: macroblock decoding input/output

### Phase 5: GPU Implementation (Weeks 9-10)

#### 5.1 GPU Command Processing
- [ ] Implement all GP0 commands:
  - NOP (00h)
  - VRAM transfers (CPU↔VRAM, VRAM↔VRAM, rectangle fills)
  - Primitive rendering (points, lines, rectangles, triangles, quads)
  - Texture page and clut operations
  - Drawing environment setup (offsets, clipping, drawing area)
  - Texture window settings
- [ ] Implement all GP1 commands:
  - Display control (display enable, mode, etc.)
  - DMA control (request, reset, etc.)
  - Interrupt control (flag setting/clearing)
  - Display environment (position, size)
  - Texture window position/size
- [ ] Implement proper command FIFO handling
- [ ] Add texture page and clut caching

#### 5.2 VRAM & Display System
- [ ] Implement 1MB VRAM with proper organization:
  - Framebuffer storage (various pixel formats)
  - Texture storage (4-bit, 8-bit, 16-bit)
  - CLUT storage
- [ ] Add framebuffer to VRAM mapping
- [ ] Implement display timing and output generation:
  - Horizontal/vertical sync
  - Color burst
  - Various display modes (interlace, etc.)
- [ ] Implement drawing area offsets and clipping
- [ ] Add texture window functionality
- [ ] Implement semi-transparency effects

### Phase 6: SPU & CDROM System (Weeks 11-12)

#### 6.1 SPU Sound Processing
- [ ] Implement 24-channel ADPCM sound generator:
  - Channel attributes (volume, pitch, ADPCM settings)
  - Key on/off handling with attack/decay/sustain/release
  - Frequency calculation and interpolation
- [ ] Add reverb and noise channels:
  - Reverb buffer and processing
  - Noise generator with frequency control
- [ ] Implement volume and pitch envelopes:
  - Linear and exponential curves
  - Sustain and release phases
- [ ] Add proper mixing and output stage:
  - Stereo output
  - Master volume control
  - Reverb mix controls
- [ ] Implement SPU memory and register access:
  - Sound RAM (512K)
  - Register interface (volume, pitch, address, etc.)

#### 6.2 CDROM Controller
- [ ] Implement accurate CDROM timing:
  - Sector reading times (1/75 second per sector)
  - Motor spin up/down delays
  - Seek times between tracks
- [ ] Add sector reading and caching:
  - Mode 1/Form 1 and Form 2 sectors
  - CD-DA audio sectors
  - Proper error detection/correction
- [ ] Implement command set:
  - Get status, get location
  - Play, forward, backward
  - Read N, stop, pause
  - Initialize, mute, demute
  - Set volume
  - Get TD (track info)
- [ ] Add IRQ generation for command completion
- [ ] Implement proper interrupt handling:
  - Command completion interrupt
  - Data ready interrupt
  - Auto-pause detection

### Phase 7: Controllers & Expansion (Weeks 13-14)

#### 7.1 Controller Interface
- [ ] Implement standard PSX controller polling:
  - Serial interface timing
  - Button state reading
  - Analog stick reading ( DualShock )
  - Pressure sensitivity (later models)
- [ ] Add multi-tap support:
  - Up to 4 controllers per port
  - Proper polling sequence
  - ID reading and configuration
- [ ] Implement memory card interface:
  - Standard memory card (128K-8M)
  - Proper command/response protocol
  - FAT-like file system on flash memory
  - DexDrive and other format support
- [ ] Add vibration support (later controllers)

#### 7.2 Expansion Port (PIO)
- [ ] Implement PIO expansion port:
  - 64-bit bidirectional data bus
  - Control signals (/INT, /RES, etc.)
  - Proper timing and synchronization
- [ ] Add support for common peripherals:
  - Link cable (for multiplayer)
  - Pocketstation
  - Other official Sony peripherals
  - Third-party accessories

## Verification and Testing Approach

At each phase, verify against:
1. **Specification Compliance**: Regular checks against psx-spx.consoledev.net
2. **Test Cases**: Known test ROMs and homebrew that exercise specific features
3. **Commercial Game Progression**: 
   - Start with simple 2D games
   - Progress to 3D games with textures
   - Test games with special hardware requirements
   - Verify timing-sensitive games
4. **Accuracy Metrics**:
   - CPU instruction timing
   - Hardware register behavior
   - Interrupt timing and latency
   - DMA transfer rates
   - GPU rendering accuracy

## Immediate Next Steps (Starting Now)

### Phase 1.1: Memory Map Enhancement
1. **Enhanced Address Translation**
   - Modify psx_translate_ram() to handle KSEG0/KSEG1 vs KUSEG caching
   - Add proper memory mirroring logic
   - Implement write queue effects on hardware register access

2. **Memory Access Functions**
   - Update psx_read32()/psx_write32() to:
     - Check for memory protection violations
     - Apply appropriate caching based on address region
     - Handle unmapped areas (cause bus errors)
     - Implement write queue behavior for hardware registers

3. **Cache and Buffer Simulation**
   - Add i-Cache structure and simulation
   - Add write queue buffer (4 words)
   - Ensure scratchpad access restrictions

Let's begin with Phase 1.1 by enhancing the memory system in psx.c and psx.h.