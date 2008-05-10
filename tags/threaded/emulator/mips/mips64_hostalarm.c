#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <time.h>

#include "cpu.h"
#include "vm.h"
#include "mips64_exec.h"
#include "mips64_memory.h"
#include "ins_lookup.h"
#include "mips64.h"
#include "mips64_cp0.h"
#include "debug.h"
#include "vp_timer.h"

static int timer_freq;
extern void host_alarm_handler(int host_signum);

#define RTC_FREQ 1024

static int rtc_fd;
static int start_rtc_timer(void)
{
   rtc_fd = open("/dev/rtc", O_RDONLY);
   if (rtc_fd < 0)
      return -1;
   if (ioctl(rtc_fd, RTC_IRQP_SET, RTC_FREQ) < 0)
   {
      fprintf(stderr, "Could not configure '/dev/rtc' to have a 1024 Hz timer. This is not a fatal\n"
              "error, but for better emulation accuracy either use a 2.6 host Linux kernel or\n"
              "type 'echo 1024 > /proc/sys/dev/rtc/max-user-freq' as root.\n");
      goto fail;
   }
   if (ioctl(rtc_fd, RTC_PIE_ON, 0) < 0)
   {
    fail:
      close(rtc_fd);
      return -1;
   }
   return 0;
}

/*host alarm*/
void mips64_init_host_alarm(void)
{
   struct sigaction act;
   struct itimerval itv;

   /* get times() syscall frequency */
   timer_freq = sysconf(_SC_CLK_TCK);

   /* timer signal */
   sigfillset(&act.sa_mask);
   act.sa_flags = 0;
#if defined (TARGET_I386) && defined(USE_CODE_COPY)
   act.sa_flags |= SA_ONSTACK;
#endif
   act.sa_handler = host_alarm_handler;
   sigaction(SIGALRM, &act, NULL);

   itv.it_interval.tv_sec = 0;
   itv.it_interval.tv_usec = 999;       /* for i386 kernel 2.6 to get 1 ms */
   itv.it_value.tv_sec = 0;
   itv.it_value.tv_usec = 10 * 1000;
   setitimer(ITIMER_REAL, &itv, NULL);
   /* we probe the tick duration of the kernel to inform the user if
      the emulated kernel requested a too high timer frequency */
   getitimer(ITIMER_REAL, &itv);

   /* XXX: force /dev/rtc usage because even 2.6 kernels may not
      have timers with 1 ms resolution. The correct solution will
      be to use the POSIX real time timers available in recent
      2.6 kernels */
   /*
      Qemu uses rtc to get 1 ms resolution timer. However, it always crashs my 
      os(arch linux (core dump) ). So I do not use rtc for timer.  (yajin)
    */

   if (itv.it_interval.tv_usec > 1000 || 0)
   {
      /* try to use /dev/rtc to have a faster timer */
      if (start_rtc_timer() < 0)
         return;
      /* disable itimer */
      itv.it_interval.tv_sec = 0;
      itv.it_interval.tv_usec = 0;
      itv.it_value.tv_sec = 0;
      itv.it_value.tv_usec = 0;
      setitimer(ITIMER_REAL, &itv, NULL);

      /* use the RTC */
      sigaction(SIGIO, &act, NULL);
      fcntl(rtc_fd, F_SETFL, O_ASYNC);
      fcntl(rtc_fd, F_SETOWN, getpid());

   }

}



