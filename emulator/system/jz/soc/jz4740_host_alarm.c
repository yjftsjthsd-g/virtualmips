

/*JZ4740 host alarm*/
#include "utils.h"
#include "mips64.h"
#include "vm.h"
#include "jz4740.h"


#include "vp_timer.h"
extern cpu_mips_t *current_cpu;

/*JZ4740 host alarm. fired once 1 ms.
It will find whether a timer has been expired. If so, run timers.
*/
void host_alarm_handler(int host_signum)
{
   if (unlikely(current_cpu->state != CPU_STATE_RUNNING))
      return;


   if (vp_timer_expired(active_timers[VP_TIMER_REALTIME], vp_get_clock(rt_clock)))
   {
      /*tell cpu we need to pause because timer out */
      current_cpu->pause_request |= CPU_INTERRUPT_EXIT;
   }

}
