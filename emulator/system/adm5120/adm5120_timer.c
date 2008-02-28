 /*
 * Copyright (C) yajin 2008 <yajinzhou@gmail.com >
 *     
 * This file is part of the virtualmips distribution. 
 * See LICENSE file for terms of the license. 
 *
 */


#define _GNU_SOURCE 
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<string.h>


#include "device.h"
#include "adm5120.h"
#include "mips64.h"


/*
ADM5120   227MHZ
That means one instruction will use about 4ns.
TIM - 1 per 640ns=160 INSTRUCTIONS.

*/

int  instructions=0;
extern  m_uint32_t sw_table[SW_INDEX_MAX];
#define MAX_INSTRUCTIONS  1
int timeout=0;
m_uint32_t time_reload;

void forced_inline virtual_adm5120_timer(cpu_mips_t *cpu)
{
  m_uint32_t tim;
  
  /*check count and compare*/
  cpu->cp0.reg[MIPS_CP0_COUNT]++;
  if (cpu->cp0.reg[MIPS_CP0_COMPARE]!=0)
    {
      if (cpu->cp0.reg[MIPS_CP0_COUNT]==cpu->cp0.reg[MIPS_CP0_COMPARE])
    {
      mips64_set_irq(cpu,MIPS_TIMER_INTERRUPT);
      mips64_update_irq_flag(cpu);
    }
    }

  if (sw_table[Timer_REG/4]&SW_TIMER_EN)
    {
      instructions++;
      if (MAX_INSTRUCTIONS<=instructions)
        {
          instructions=0;
          tim = sw_table[Timer_REG/4]&SW_TIMER_MASK;
          if (tim==0)
            {
              tim=time_reload;
              timeout=1;
              if (!sw_table[Timer_int_REG/4]&SW_TIMER_INT_DISABLE)
                {
                  cpu->vm->set_irq(cpu->vm,INT_LVL_TIMER);
                }
                
            }
          else
            tim--;
          sw_table[Timer_REG/4] &= ~SW_TIMER_MASK;
          sw_table[Timer_REG/4] += tim;
          
        }
      
    }
}

