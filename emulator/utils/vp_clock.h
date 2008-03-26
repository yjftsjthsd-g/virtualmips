#ifndef __VP_CLOCK_H__
#define __VP_CLOCK_H__

#include "utils.h"
/*clock utils from qemu*/

#define VP_TIMER_REALTIME 0
#define VP_TIMER_VIRTUAL  1

struct vp_clock {
    int type;
    /* XXX: add frequency */
};
typedef struct vp_clock vp_clock_t;

extern vp_clock_t *rt_clock;
extern vp_clock_t *vm_clock;

m_int64_t vp_get_clock(vp_clock_t *clock);

#endif



