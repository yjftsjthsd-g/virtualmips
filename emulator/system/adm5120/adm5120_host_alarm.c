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



#define ADM_FREQ  175000000  /*175MHZ*/


int  instructions=0;
extern  m_uint32_t sw_table[SW_INDEX_MAX];
#define MAX_INSTRUCTIONS  1
int timeout=0;
m_uint32_t time_reload;
extern m_uint32_t uart_table[2][UART_INDEX_MAX];
extern cpu_mips_t *current_cpu;

/*ADM5120 use host_alarm_handler to process all the things,
not likely jz4740.
This method is deprecated.*/

 void host_alarm_handler(int host_signum)
{

	 m_uint32_t tim;
	 if (unlikely(current_cpu->state!=CPU_STATE_RUNNING))
		return;
	 		 if ((uart_table[0][UART_CR_REG/4] &UART_RX_INT_EN)&&(uart_table[0][UART_CR_REG/4] &UART_PORT_EN))
      {
      if (vtty_is_char_avail(current_cpu->vm->vtty_con1))
      	{
      	        uart_set_interrupt(current_cpu,0);
        uart_table[0][UART_ICR_REG/4] |= UART_RX_INT;
        return;
      	}

      }

	   	if (uart_table[0][UART_CR_REG/4]&UART_PORT_EN)
	{

		if (uart_table[0][UART_CR_REG/4]&UART_TX_INT_EN)
		{
			 uart_table[0][UART_ICR_REG/4] |= UART_TX_INT;
            uart_set_interrupt(current_cpu,0);
              return;
		}


	}


  /*check count and compare*/
  /*Why 2*1000? CPU is 175MHZ, we assume CPI(cycle per instruction)=2 
 see arch/mips/adm5120/setup.c for more information
 49 void __init mips_time_init(void)
*/
  current_cpu->cp0.reg[MIPS_CP0_COUNT]+=ADM_FREQ/(2*1000);
  if (current_cpu->cp0.reg[MIPS_CP0_COMPARE]!=0)
    {
    cpu_log6(current_cpu,"","count %x compare %x \n",current_cpu->cp0.reg[MIPS_CP0_COUNT],current_cpu->cp0.reg[MIPS_CP0_COMPARE]);
if (current_cpu->cp0.reg[MIPS_CP0_COUNT]>=current_cpu->cp0.reg[MIPS_CP0_COMPARE])
    {
      mips64_set_irq(current_cpu,MIPS_TIMER_INTERRUPT);
      mips64_update_irq_flag(current_cpu);
    }
    }

   /*Linux kernel does not use this timer. It use mips count*/
	 if (sw_table[Timer_REG/4]&SW_TIMER_EN)
    {
          tim = sw_table[Timer_REG/4]&SW_TIMER_MASK;
          cpu_log(current_cpu,"","tim %x \n",tim);
          if (tim==0)
            {
              tim=time_reload;
              timeout=1;
          }
          else
            tim-=0x2000; /*1ms=2000*640ns.but 2000 is too slow. I set it to 0x2000*/
          if ((m_int32_t)tim<0x2000)
          	tim=0;
          sw_table[Timer_REG/4] &= ~SW_TIMER_MASK;
          sw_table[Timer_REG/4] += tim;
          

      
    }

	 
	 	
}
#if 0

void host_alarm_handler(int host_signum)
{
   m_uint32_t tim;
  
  /*check count and compare*/
  current_cpu->cp0.reg[MIPS_CP0_COUNT]+=0x15500;
  if (current_cpu->cp0.reg[MIPS_CP0_COMPARE]!=0)
    {
    cpu_log4(current_cpu,"","count %x compare %x \n",current_cpu->cp0.reg[MIPS_CP0_COUNT],current_cpu->cp0.reg[MIPS_CP0_COMPARE]);
      if (current_cpu->cp0.reg[MIPS_CP0_COUNT]>=current_cpu->cp0.reg[MIPS_CP0_COMPARE])
    {
      mips64_set_irq(current_cpu,MIPS_TIMER_INTERRUPT);
      mips64_update_irq_flag(current_cpu);
    }
    }
   
	 if (sw_table[Timer_REG/4]&SW_TIMER_EN)
    {
      //instructions++;
     // if (MAX_INSTRUCTIONS<=instructions)
        {
          //instructions=0;
          tim = sw_table[Timer_REG/4]&SW_TIMER_MASK;
          if (tim==0)
            {
              tim=time_reload;
              timeout=1;
              if (!sw_table[Timer_int_REG/4]&SW_TIMER_INT_DISABLE)
                {
                  current_cpu->vm->set_irq(current_cpu->vm,INT_LVL_TIMER);
                }
                
            }
          else
            tim-=500; /*1ms=2*640ns*/
          sw_table[Timer_REG/4] &= ~SW_TIMER_MASK;
          sw_table[Timer_REG/4] += tim;
          
        }
      
    }
}




void virtual_adm5120_timer(cpu_mips_t *cpu)
{
  m_uint32_t tim;
  
  
  if (cpu->cp0.reg[MIPS_CP0_COMPARE]!=0)
    {
     /*check count and compare*/
  cpu->cp0.reg[MIPS_CP0_COUNT]+=0x1;
      if (cpu->cp0.reg[MIPS_CP0_COUNT]>=cpu->cp0.reg[MIPS_CP0_COMPARE])
    {
      mips64_set_irq(cpu,MIPS_TIMER_INTERRUPT);
      mips64_update_irq_flag(cpu);
    }
    }
/*
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
      
    }*/
}

void virtual_timer(cpu_mips_t *cpu)
{
//virtual_adm5120_timer(cpu);
}


#endif

