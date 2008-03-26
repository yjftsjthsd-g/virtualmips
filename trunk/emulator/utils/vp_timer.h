#ifndef __VP_TIMER_H__
#define __VP_TIMER_H__

#include "vp_clock.h"
#include "utils.h"

#define VP_TIMER_BASE 1000000000LL

typedef void vp_timer_cb(void *opaque);

struct vp_timer {
    vp_clock_t *clock;
    m_int64_t expire_time;
    m_int64_t set_time;
    vp_timer_cb *cb;
    void *opaque;
    struct vp_timer *next;
};
typedef struct vp_timer vp_timer_t;
extern vp_timer_t *active_timers[2];


#endif


