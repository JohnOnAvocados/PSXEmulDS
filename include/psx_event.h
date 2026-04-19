#ifndef PSX_EVENT_H
#define PSX_EVENT_H

#include <stdint.h>
#include <stdbool.h>

#include "psx.h"

#define PSX_EVENT_MAX_CALLBACKS 32
#define PSX_EVENT_TIMER_COUNT 3

#define PSX_EVENT_FLAG_CB  0x0001
#define PSX_EVENT_FLAG_IO 0x0002
#define PSX_EVENT_FLAG_PIO 0x0004
#define PSX_EVENT_flag_T0 0x0008
#define PSX_EVENT_FLAG_T1 0x0010
#define PSX_EVENT_FLAG_T2 0x0020
#define PSX_EVENT_FLAG_SIO 0x0040
#define PSX_EVENT_FLAG_W0 0x0100
#define PSX_EVENT_FLAG_W1 0x0200
#define PSX_EVENT_FLAG_W2 0x0400
#define PSX_EVENT_FLAG_RTC 0x0800
#define PSX_EVENT_FLAG_CD 0x1000
#define PSX_EVENT_FLAG_PAD 0x2000
#define PSX_EVENT_FLAG_MC 0x4000

typedef struct {
    void *next;
    void *prev;
    uint32_t status;
    uint32_t mask;
    uint32_t (*handler)(uint32_t, void *);
    void *userdata;
} PsxEventCallback;

typedef struct {
    PsxEventCallback *first;
    PsxEventCallback *last;
    uint32_t count;
} PsxEventQueue;

typedef struct {
    uint32_t handler_addr;
    uint32_t handler_stack;
    uint32_t handler_gpr[4];
} PsxEventCtx;

typedef enum {
    PSX_EVENT_TYPE_TIMER = 0,
    PSX_EVENT_TYPE_CDROM,
    PSX_EVENT_TYPE_DMA,
    PSX_EVENT_TYPE_GPU,
    PSX_EVENT_TYPE_SPU,
    PSX_EVENT_TYPE_PAD,
    PSX_EVENT_TYPE_MEMCARD,
    PSX_EVENT_TYPE_SIO,
    PSX_EVENT_TYPE_COUNT
} PsxEventType;

void psx_event_init(PsxState *psx);
void psx_event_reset(PsxState *psx);
void psx_event_trigger(PsxState *psx, PsxEventType type, uint32_t param);
int psx_event_add_callback(PsxState *psx, PsxEventType type, uint32_t (*handler)(uint32_t, void *), void *userdata);
void psx_event_remove_callback(PsxState *psx, int handle_id);
void psx_event_enable(PsxState *psx, PsxEventType type);
void psx_event_disable(PsxState *psx, PsxEventType type);
int psx_event_open(PsxState *psx, int cb_type, int initial_count);
int psx_event_close(PsxState *psx, int fd);
int psx_event_wait(PsxState *psx, int fd, bool wait_for_all, uint32_t timeout);
int psx_event_wait_async(PsxState *psx, int fd);
void psx_event_cancel(PsxState *psx, int fd, uint32_t reason);
void psx_deliver_event(PsxState *psx);

void psx_event_init_timers(PsxState *psx);
void psx_event_update_timers(PsxState *psx, uint32_t cycles);
int psx_event_set_timer(PsxState *psx, int timer_num, uint32_t mode, uint32_t count, uint32_t target);
void psx_event_get_timer(PsxState *psx, int timer_num, uint32_t *mode, uint32_t *count, uint32_t *target);

uint32_t psx_interrupt_handler(PsxState *psx, uint32_t irq_mask);
void psx_interrupt_reset(PsxState *psx);
void psx_interrupt_set_mode(PsxState *psx, uint32_t mode);
uint32_t psx_interrupt_get_mode(const PsxState *psx);

void psx_handle_interrupt(PsxState *psx);

#endif