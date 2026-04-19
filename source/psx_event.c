#include "psx.h"
#include "psx_event.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PSX_EVENT_TIMER0_IRQ  (1 << 4)
#define PSX_EVENT_TIMER1_IRQ (1 << 5)
#define PSX_EVENT_TIMER2_IRQ (1 << 6)

typedef struct {
    uint32_t count;
    uint32_t compare;
    uint32_t mode;
    uint32_t target;
} PsxHwTimer;

static PsxHwTimer g_timers[3];
static PsxEventQueue g_event_queues[PSX_EVENT_TYPE_COUNT];
static uint32_t g_int_mode = 0;
static uint32_t g_int_enable = 0;
static uint32_t g_pending_events = 0;

void psx_event_init(PsxState *psx) {
    (void)psx;
    memset(g_timers, 0, sizeof(g_timers));
    memset(g_event_queues, 0, sizeof(g_event_queues));
    g_int_mode = 0;
    g_int_enable = 0;
    g_pending_events = 0;
}

void psx_event_reset(PsxState *psx) {
    psx_event_init(psx);
    for (int i = 0; i < PSX_EVENT_TYPE_COUNT; i++) {
        PsxEventCallback *cb = g_event_queues[i].first;
        while (cb) {
            PsxEventCallback *next = cb->next;
            free(cb);
            cb = next;
        }
        memset(&g_event_queues[i], 0, sizeof(PsxEventQueue));
    }
}

void psx_event_trigger(PsxState *psx, PsxEventType type, uint32_t param) {
    (void)psx;
    if (type >= PSX_EVENT_TYPE_COUNT) {
        return;
    }
    g_pending_events |= (1 << type);
    
    PsxEventCallback *cb = g_event_queues[type].first;
    while (cb) {
        if ((cb->status & 0x01) && (cb->mask & g_int_enable)) {
            cb->handler(param, cb->userdata);
        }
        cb = cb->next;
    }
}

int psx_event_add_callback(PsxState *psx, PsxEventType type, uint32_t (*handler)(uint32_t, void *), void *userdata) {
    (void)psx;
    if (type >= PSX_EVENT_TYPE_COUNT || handler == NULL) {
        return -1;
    }
    
    PsxEventCallback *cb = (PsxEventCallback *)malloc(sizeof(PsxEventCallback));
    if (cb == NULL) {
        return -1;
    }
    
    memset(cb, 0, sizeof(PsxEventCallback));
    cb->handler = handler;
    cb->userdata = userdata;
    cb->status = 0x01;
    cb->mask = 0xFFFFFFFF;
    
    if (g_event_queues[type].last) {
        g_event_queues[type].last->next = cb;
        cb->prev = g_event_queues[type].last;
        g_event_queues[type].last = cb;
    } else {
        g_event_queues[type].first = cb;
        g_event_queues[type].last = cb;
    }
    g_event_queues[type].count++;
    
    return 0;
}

void psx_event_remove_callback(PsxState *psx, int handle_id) {
    (void)psx;
    (void)handle_id;
}

void psx_event_enable(PsxState *psx, PsxEventType type) {
    (void)psx;
    if (type >= PSX_EVENT_TYPE_COUNT) {
        return;
    }
    g_int_enable |= (1 << type);
}

void psx_event_disable(PsxState *psx, PsxEventType type) {
    (void)psx;
    if (type >= PSX_EVENT_TYPE_COUNT) {
        return;
    }
    g_int_enable &= ~(1 << type);
}

int psx_event_open(PsxState *psx, int cb_type, int initial_count) {
    (void)psx;
    if (cb_type < 0 || cb_type >= PSX_EVENT_TYPE_COUNT) {
        return -1;
    }
    if (initial_count <= 0 || initial_count > 32) {
        initial_count = 1;
    }
    return cb_type;
}

int psx_event_close(PsxState *psx, int fd) {
    (void)psx;
    (void)fd;
    return 0;
}

int psx_event_wait(PsxState *psx, int fd, bool wait_for_all, uint32_t timeout) {
    (void)psx;
    (void)fd;
    (void)wait_for_all;
    (void)timeout;
    return 0;
}

int psx_event_wait_async(PsxState *psx, int fd) {
    (void)psx;
    (void)fd;
    return 0;
}

void psx_event_cancel(PsxState *psx, int fd, uint32_t reason) {
    (void)psx;
    (void)fd;
    (void)reason;
}

void psx_deliver_event(PsxState *psx) {
    (void)psx;
}

void psx_event_init_timers(PsxState *psx) {
    (void)psx;
    memset(g_timers, 0, sizeof(g_timers));
}

void psx_event_update_timers(PsxState *psx, uint32_t cycles) {
    (void)psx;
    for (int i = 0; i < 3; i++) {
        if ((g_timers[i].mode & 0x80) == 0) {
            continue;
        }
        
        g_timers[i].count += cycles;
        uint32_t target = g_timers[i].target;
        if (target == 0) {
            target = 0xFFFF;
        }
        
        if (g_timers[i].count >= target) {
            uint32_t irq_mask = 0;
            switch (i) {
                case 0: irq_mask = PSX_EVENT_TIMER0_IRQ; break;
                case 1: irq_mask = PSX_EVENT_TIMER1_IRQ; break;
                case 2: irq_mask = PSX_EVENT_TIMER2_IRQ; break;
            }
            
            if (g_timers[i].mode & 0x0400) {
                while (g_timers[i].count >= target) {
                    g_timers[i].count -= target;
                }
            } else {
                g_timers[i].count = 0;
                g_timers[i].mode &= ~0x80;
            }
        }
    }
}

int psx_event_set_timer(PsxState *psx, int timer_num, uint32_t mode, uint32_t count, uint32_t target) {
    (void)psx;
    if (timer_num < 0 || timer_num > 2) {
        return -1;
    }
    g_timers[timer_num].mode = mode;
    g_timers[timer_num].count = count;
    g_timers[timer_num].target = target;
    g_timers[timer_num].compare = (mode & 0x08) ? 0xFFFFFFFF : 0xFFFF;
    return 0;
}

void psx_event_get_timer(PsxState *psx, int timer_num, uint32_t *mode, uint32_t *count, uint32_t *target) {
    (void)psx;
    if (timer_num < 0 || timer_num > 2) {
        return;
    }
    if (mode) {
        *mode = g_timers[timer_num].mode;
    }
    if (count) {
        *count = g_timers[timer_num].count;
    }
    if (target) {
        *target = g_timers[timer_num].target;
    }
}

uint32_t psx_interrupt_handler(PsxState *psx, uint32_t irq_mask) {
    (void)psx;
    uint32_t handled = 0;
    
    if (irq_mask & PSX_EVENT_TIMER0_IRQ) {
        handled |= PSX_EVENT_TIMER0_IRQ;
    }
    if (irq_mask & PSX_EVENT_TIMER1_IRQ) {
        handled |= PSX_EVENT_TIMER1_IRQ;
    }
    if (irq_mask & PSX_EVENT_TIMER2_IRQ) {
        handled |= PSX_EVENT_TIMER2_IRQ;
    }
    
    return handled;
}

void psx_interrupt_reset(PsxState *psx) {
    (void)psx;
    g_int_mode = 0;
    g_int_enable = 0;
    g_pending_events = 0;
}

void psx_interrupt_set_mode(PsxState *psx, uint32_t mode) {
    (void)psx;
    g_int_mode = mode;
}

uint32_t psx_interrupt_get_mode(const PsxState *psx) {
    (void)psx;
    return g_int_mode;
}

void psx_handle_interrupt(PsxState *psx) {
    uint32_t status = psx->cpu.cop0[12];
    uint32_t cause = psx->cpu.cop0[13];
    
    if ((status & 0x01) == 0) {
        return;
    }
    
    uint32_t irq_mask = (status >> 8) & 0xFF;
    uint32_t pending = cause & 0x0000FF00;
    
    if (pending == 0) {
        return;
    }
    
    uint32_t old_status = status;
    status = (status & ~0x0000003C) | ((status & 0x0000000F) << 2);
    psx->cpu.cop0[12] = status;
    psx->cpu.pc = psx->cpu.next_pc;
    psx->cpu.next_pc = 0x80000080;
    psx->cpu.cop0[12] = old_status | 0x00000002;
    
    psx->cycles++;
}