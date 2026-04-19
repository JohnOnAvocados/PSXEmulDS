#ifndef PSX_H
#define PSX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PSX_RAM_SIZE (2 * 1024 * 1024)  // PSX has 2MB RAM
#define PSX_BIOS_SIZE (512 * 1024)
#define PSX_SCRATCHPAD_SIZE 1024
#define PSX_IO_SIZE 4096
#define PSX_TRACE_LINES 2

#define PSX_IRQ_VBLANK    (1 << 0)
#define PSX_IRQ_GPU       (1 << 1)
#define PSX_IRQ_CDROM     (1 << 2)
#define PSX_IRQ_DMA       (1 << 3)
#define PSX_IRQ_TIMER0    (1 << 4)
#define PSX_IRQ_TIMER1    (1 << 5)
#define PSX_IRQ_TIMER2    (1 << 6)
#define PSX_IRQ_SPU       (1 << 7)
#define PSX_IRQ_EXTERNAL  (1 << 8)

#define PSX_TIMER_MODE_IRQ    (1 << 10)
#define PSX_TIMER_MODE_REPEAT (1 << 9)
#define PSX_TIMER_MODE_32BIT (1 << 8)

// Memory region constants
#define PSX_KSEG0_BASE 0x80000000
#define PSX_KSEG1_BASE 0xA0000000
#define PSX_KSEG2_BASE 0xC0000000
#define PSX_KUSEG_BASE 0x00000000

// Memory mirroring
#define PSX_RAM_MIRROR_SIZE 0x00800000  // 2MB RAM mirrors to first 8MB
#define PSX_BIOS_MIRROR_SIZE 0x00400000 // 512K BIOS mirrors to last 4MB

// Cache constants
#define PSX_ICACHE_SIZE 4096
#define PSX_ICACHE_LINE_SIZE 16
#define PSX_ICACHE_LINE_COUNT (PSX_ICACHE_SIZE / PSX_ICACHE_LINE_SIZE)

// Write queue
#define PSX_WRITE_QUEUE_SIZE 4

// COP0 register indices
#define PSX_COP0_INDEX     0
#define PSX_COP0_RANDOM   1
#define PSX_COP0_BPC    3
#define PSX_COP0_BDA    4
#define PSX_COP0_JUMP  5
#define PSX_COP0_TLB   9
#define PSX_COP0_PIDX  10
#define PSX_COP0_TLBHI 12
#define PSX_COP0_TLBLO 13
#define PSX_COP0_TLBIX 14
#define PSX_COP0_TLBHI 12
#define PSX_COP0_PIDX  10
#define PSX_COP0_BADV   8
#define PSX_COP0_SR    12
#define PSX_COP0_CAUSE 13
#define PSX_COP0_EPC  14
#define PSX_COP0_PRID 15
#define PSX_COP0_CONFIG 16
#define PSX_COP0_WATCHLO 18
#define PSX_COP0_WATCHHI 19

// COP0 Status register bits
#define PSX_COP0_SR_IE   0x00000001
#define PSX_COP0_SR_EXL  0x00000002
#define PSX_COP0_SR_ERL  0x00000004
#define PSX_COP0_SR_IM0  0x00000100
#define PSX_COP0_SR_IM1  0x00000200
#define PSX_COP0_SR_IM2  0x00000400
#define PSX_COP0_SR_IM3  0x00000800
#define PSX_COP0_SR_IM4  0x00001000
#define PSX_COP0_SR_IM5  0x00002000
#define PSX_COP0_SR_IM6  0x00004000
#define PSX_COP0_SR_IM7  0x00008000
#define PSX_COP0_SR_DE  0x00010000

// COP0 Cause register bits
#define PSX_COP0_CAUSE_EXC  0x0000003C
#define PSX_COP0_CAUSE_IP   0x0000FF00
#define PSX_COP0_CAUSE_CE  0x30000000
#define PSX_COP0_CAUSE_BD  0x00010000

// Exception codes
#define PSX_EXC_INT    0x00
#define PSX_EXC_MOD    0x04
#define PSX_EXC_TLBL   0x08
#define PSX_EXC_TLBS   0x0C
#define PSX_EXC_ADEL   0x10
#define PSX_EXC_ADES   0x14
#define PSX_EXC_IBE    0x20
#define PSX_EXC_DBE    0x24
#define PSX_EXC_SYS    0x28
#define PSX_EXC_BP     0x2C
#define PSX_EXC_RI     0x30
#define PSX_EXC_OV     0x34

typedef struct {
    uint32_t gpr[32];
    uint32_t cop0[32];  // Complete COP0 register set
    uint32_t hi;
    uint32_t lo;
    uint32_t pc;
    uint32_t next_pc;
    uint32_t delay_slot;  // PC of delay slot instruction
    bool in_delay_slot;
} PsxCpuState;

typedef struct {
    uint32_t count;
    uint32_t compare;
    uint32_t mode;
    uint32_t target;
} PsxTimer;

typedef struct {
    uint16_t error_code;
    uint16_t test_count;
    uint32_t cycles_at_error;
    uint32_t pc_at_error;
    uint32_t opcode_at_error;
    char error_description[128];
} PsxTestResult;

// i-Cache line structure
typedef struct {
    uint32_t tag;          // Physical address tag
    uint8_t valid;         // Valid bits for 4 words in line
    uint8_t data[PSX_ICACHE_LINE_SIZE]; // Cached data
} PsxICacheLine;

// Write queue entry
typedef struct {
    uint32_t address;
    uint32_t value;
    uint8_t size;          // 1=byte, 2=halfword, 4=word
    bool is_write;
} PsxWriteQueueEntry;

#include "psx_gpu.h"
#include "psx_dma.h"
#include "psx_cdrom.h"

typedef struct {
    PsxCpuState cpu;
    uint8_t ram_internal[PSX_RAM_SIZE];
    uint8_t *ram;
    size_t ram_size;
    char ram_backend_name[16];
    uint8_t bios[PSX_BIOS_SIZE];
    uint8_t scratchpad[PSX_SCRATCHPAD_SIZE];
    uint32_t io_regs[PSX_IO_SIZE / sizeof(uint32_t)];
    uint64_t cycles;
    bool halted;
    bool bios_loaded;
    uint32_t last_pc;
    uint32_t last_opcode;
    uint32_t halt_pc;
    char halt_reason[64];
    char last_disasm[64];
    char trace[PSX_TRACE_LINES][64];
    uint32_t trace_pos;
    uint32_t trace_count;
    uint32_t last_io_addr;
    uint32_t last_io_value;
    bool last_io_write;
    uint16_t pending_irqs;
    PsxTimer timers[3];
    PsxTestResult test_result;
    bool test_mode;
    struct PsxGpuState *gpu;
    struct PsxDmaState *dma;
    struct PsxCdromState *cdrom;
    uint32_t vblank_counter;
    
    // Memory system enhancements
    PsxICacheLine icache[PSX_ICACHE_LINE_COUNT];
    PsxWriteQueueEntry write_queue[PSX_WRITE_QUEUE_SIZE];
    int write_queue_head;
    int write_queue_tail;
    int write_queue_count;
} PsxState;

void psx_init(PsxState *psx);
void psx_reset(PsxState *psx);
void psx_load_demo(PsxState *psx);
void psx_load_raw_bin(PsxState *psx, const uint8_t *data, size_t size, uint32_t load_addr, uint32_t entry_pc);
bool psx_load_bios(PsxState *psx, const uint8_t *data, size_t size);
void psx_boot_bios(PsxState *psx);
void psx_use_internal_ram(PsxState *psx);
void psx_use_slot2_ram(PsxState *psx, uint8_t *slot2_buffer, size_t slot2_size);
bool psx_set_ram_backing(PsxState *psx, uint8_t *buffer, size_t size, const char *backend_name);
void psx_step(PsxState *psx);
uint32_t psx_run(PsxState *psx, uint32_t max_steps);
uint32_t psx_read32(const PsxState *psx, uint32_t addr);
void psx_write32(PsxState *psx, uint32_t addr, uint32_t value);
void psx_trigger_irq(PsxState *psx, uint16_t irq_mask);
void psx_acknowledge_irq(PsxState *psx, uint16_t irq_mask);
bool psx_check_irq_pending(const PsxState *psx);
void psx_step_timers(PsxState *psx, uint32_t cycles);
void psx_init_test_mode(PsxState *psx);
void psx_record_error(PsxState *psx, uint16_t error_code, const char *description);
void psx_init_gpu(PsxState *psx);
void psx_init_dma(PsxState *psx);
void psx_init_cdrom(PsxState *psx);
void psx_update_peripherals(PsxState *psx, uint32_t cycles);
void psx_render_frame(PsxState *psx);

#endif
