#pragma once

#include <err-codes.h>
#include <window-sys.h>

static inline bool px_we_queue_empty(PX_WE_Queue* q) {
    return q->head == q->tail;
}

static inline bool px_we_queue_full(PX_WE_Queue* q) {
    return ((q->head + 1) % PX_WE_QUEUE_SIZE) == q->tail;
}

static inline bool px_we_queue_push(PX_WE_Queue* q, PX_WEvent* ev) {
    if (px_we_queue_full(q)) return false;
    q->events[q->head] = *ev;
    q->head = (q->head + 1) % PX_WE_QUEUE_SIZE;
    return true;
}

static inline bool px_we_queue_pop(PX_WE_Queue* q, PX_WEvent* ev) {
    if (px_we_queue_empty(q)) return false;
    *ev = q->events[q->tail];
    q->tail = (q->tail + 1) % PX_WE_QUEUE_SIZE;
    return true;
}

typedef struct px_ws_backend {
    t_err_codes (*init)(void);
    void (*shutdown)(void);

    t_err_codes (*create)(PX_Window*);
    void (*destroy)(PX_Window*);

    t_err_codes (*show)(PX_Window*);
    t_err_codes (*hide)(PX_Window*);

    t_err_codes (*poll_events)(PX_Window*);

    t_err_codes (*show_splash)(void);
    t_err_codes (*window_design)(PX_Window*, PX_WindowDesign*);
    
    t_err_codes (*create_ctx)(PX_Window*);
    t_err_codes (*swap_buffers)(PX_Window*);
} t_px_ws_backend;

