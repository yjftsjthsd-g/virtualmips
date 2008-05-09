 /*
   * Copyright (C) yajin 2008 <yajinzhou@gmail.com >
   *     
   * This file is part of the virtualmips distribution. 
   * See LICENSE file for terms of the license. 
   *
   */



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
cpu_mips_t *current_cpu;



#ifdef _USE_ILT_
/* Forward declaration of instruction array */
static struct mips64_insn_exec_tag mips64_exec_tags[];
static insn_lookup_t *ilt = NULL;
static void *mips64_exec_get_insn(int index);
#else
static struct mips64_op_desc mips_opcodes[];
static struct mips64_op_desc mips_spec_opcodes[];
static struct mips64_op_desc mips_bcond_opcodes[];
static struct mips64_op_desc mips_cop0_opcodes[];
static struct mips64_op_desc mips_mad_opcodes[];
static struct mips64_op_desc mips_tlb_opcodes[];
#endif

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
static void mips64_init_host_alarm(void)
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



/* Execute a memory operation (2) */
static forced_inline int mips64_exec_memop2(cpu_mips_t * cpu, int memop,
                                            m_va_t base, int offset, u_int dst_reg, int keep_ll_bit)
{
   m_va_t vaddr = cpu->gpr[base] + sign_extend(offset, 16);
   mips_memop_fn fn;

   if (!keep_ll_bit)
      cpu->ll_bit = 0;
   fn = cpu->mem_op_fn[memop];
   return (fn(cpu, vaddr, dst_reg));
}


/* Fetch an instruction */
static forced_inline int mips64_exec_fetch(cpu_mips_t * cpu, m_va_t pc, mips_insn_t * insn)
{
   m_va_t exec_page;
   m_uint32_t offset;

   exec_page = pc & ~(m_va_t) MIPS_MIN_PAGE_IMASK;
   if (unlikely(exec_page != cpu->njm_exec_page))
   {
      cpu->njm_exec_page = exec_page;
      cpu->njm_exec_ptr = cpu->mem_op_lookup(cpu, exec_page);
   }

   if (cpu->njm_exec_ptr == NULL)
   {
      //exception when fetching instruction
      return (1);
   }

   offset = (pc & MIPS_MIN_PAGE_IMASK) >> 2;
   *insn = vmtoh32(cpu->njm_exec_ptr[offset]);
   return (0);

}



/*for emulation performance check*/
#ifdef DEBUG_MHZ
#define C_1000MHZ 1000000000
struct timeval pstart, pend;
float timeuse, performance;
m_uint64_t instructions_executed = 0;
#endif

/* Execute a single instruction */
static forced_inline int mips64_exec_single_instruction(cpu_mips_t * cpu, mips_insn_t instruction)
{

#ifdef DEBUG_MHZ
   if (unlikely(instructions_executed == 0))
   {
      gettimeofday(&pstart, NULL);
   }
   instructions_executed++;
   if (unlikely(instructions_executed == C_1000MHZ))
   {
      gettimeofday(&pend, NULL);
      timeuse = 1000000 * (pend.tv_sec - pstart.tv_sec) + pend.tv_usec - pstart.tv_usec;
      timeuse /= 1000000;
      performance = 1000 / timeuse;
      printf("Used Time:%f seconds.  %f MHZ\n", timeuse, performance);
      exit(1);
   }
#endif

#ifdef _USE_ILT_
/*use instruction lookup table for instruction decoding.
from dynamips.
*/
   register int (*exec) (cpu_mips_t *, mips_insn_t) = NULL;
   struct mips64_insn_exec_tag *tag;
   int index;

   /* Lookup for instruction */

   index = ilt_lookup(ilt, instruction);
   tag = mips64_exec_get_insn(index);
   exec = tag->exec;

   return (exec(cpu, instruction));

#else

   register uint op;
   op = MAJOR_OP(instruction);

   return mips_opcodes[op].func(cpu, instruction);
#endif

}



void mips64_main_loop_wait(cpu_mips_t * cpu, int timeout)
{
   vp_run_timers(&active_timers[VP_TIMER_REALTIME], vp_get_clock(rt_clock));
}

/* Run MIPS code in step-by-step mode */
void *mips64_exec_run_cpu(cpu_mips_t * cpu)
{
   mips_insn_t insn = 0;
   int res;


   cpu->cpu_thread_running = TRUE;
   current_cpu = cpu;

   mips64_init_host_alarm();

 start_cpu:

   for (;;)
   {
      if (unlikely(cpu->state != CPU_STATE_RUNNING))
         break;

      /*virtual clock for cpu. */
      /*We do not need this anymore.
         Work has been done in host alarm */
      /*virtual_timer(); */

      if (unlikely((cpu->pause_request) & CPU_INTERRUPT_EXIT))
      {
         cpu->state = CPU_STATE_PAUSING;
         break;
      }


      /* Reset "zero register" (for safety) */
      cpu->gpr[0] = 0;

      /* Check IRQ */
      if (unlikely(cpu->irq_pending))
      {
         mips64_trigger_irq(cpu);
         continue;
      }
      /* Fetch and execute the instruction */
      res = mips64_exec_fetch(cpu, cpu->pc, &insn);

      if (unlikely(res == 1))
      {
         /*exception when fetching instruction */
         continue;
      }
      if ((cpu->vm->mipsy_debug_mode)
          && ((cpu_hit_breakpoint(cpu->vm, cpu->pc) == SUCCESS) || (cpu->vm->gdb_interact_sock == -1)
              || (cpu->vm->mipsy_break_nexti == MIPS_BREAKANYCPU)))
      {
         if (mips_debug(cpu->vm, 1))
         {
            continue;
         }
      }

      res = mips64_exec_single_instruction(cpu, insn);

      /* Normal flow ? */
      if (likely(!res))
         cpu->pc += sizeof(mips_insn_t);

   }

   while (cpu->cpu_thread_running)
   {
      switch (cpu->state)
      {
      case CPU_STATE_RUNNING:
         cpu->state = CPU_STATE_RUNNING;
         goto start_cpu;

      case CPU_STATE_HALTED:
         cpu->cpu_thread_running = FALSE;
         break;
      case CPU_STATE_RESTARTING:
         cpu->state = CPU_STATE_RESTARTING;
         /*Just waiting for cpu restart. */
         break;
      case CPU_STATE_PAUSING:
         /*main loop must wait for me. heihei :) */
         mips64_main_loop_wait(cpu, 0);
         cpu->state = CPU_STATE_RUNNING;
         cpu->pause_request &= ~CPU_INTERRUPT_EXIT;
         /*start cpu again */
         goto start_cpu;

      }
   }
   return NULL;
}

/* Execute the instruction in delay slot */
static forced_inline int mips64_exec_bdslot(cpu_mips_t * cpu)
{
   mips_insn_t insn;
   int res = 0;
   cpu->is_in_bdslot = 1;

   /* Fetch the instruction in delay slot */
   res = mips64_exec_fetch(cpu, cpu->pc + 4, &insn);
   if (res == 1)
   {
      /*exception when fetching instruction */
      cpu->is_in_bdslot = 0;
      return (1);
   }
   cpu->is_in_bdslot = 1;
   /* Execute the instruction */
   res = mips64_exec_single_instruction(cpu, insn);
   cpu->is_in_bdslot = 0;
   return res;
}




#ifdef _USE_ILT_
/*Which method to decode instruction.
ILT is good for debuging but not efficient.*/
#include "mips64_ilt.h"
#else
/*The default one*/
#include "mips64_codetable.h"
#endif


