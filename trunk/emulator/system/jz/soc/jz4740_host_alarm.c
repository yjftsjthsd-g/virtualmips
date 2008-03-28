

/*JZ4740 host alarm*/
#include "utils.h"
#include "mips64.h"
#include "vm.h"
#include "jz4740.h"

#include "jz4740_dev_uart.h"
#include "vp_timer.h"
extern cpu_mips_t *current_cpu;

/*JZ4740 host alarm. fired once 1 ms.
It will find whether a timer has been expired. If so, run timers.
*/
void host_alarm_handler(int host_signum)
{
if (unlikely(current_cpu->state!=CPU_STATE_RUNNING))
		return;

//if (current_cpu->pause_request ==CPU_INTERRUPT_EXIT)
//	return;

	 if (vp_timer_expired(active_timers[VP_TIMER_REALTIME],vp_get_clock(rt_clock))) 
	 	{
	 		/*tell cpu we need to pause because timer out*/
	 		current_cpu->pause_request |= CPU_INTERRUPT_EXIT;
	 	 //mips64_pause(current_cpu, CPU_INTERRUPT_EXIT);
	 	//cpu_log7(current_cpu,"","host_alarm_handler \n");
	 	//vp_run_timers(&active_timers[VP_TIMER_REALTIME], 
       //             vp_get_clock(rt_clock));
	 	}
	 	 
}


#if 0
/*The interface of JZ4740 TIMER
MUST define function host_alarm_handler,which will be trigged once 1ms.
*/

extern m_uint32_t jz4740_wdt_tcu_table[JZ4740_WDT_INDEX_MAX];
extern m_uint64_t jz4740_tcu_clock[JZ4740_WDT_INDEX_MAX];

m_uint64_t last_time;
m_uint64_t current_time;
/*This function is fired once 1 ms. 
put timer/wdt and uart work here.
*/


void host_alarm_handler(int host_signum)
{
//cpu_log9(current_cpu,"","clock %llx \n",get_clock());	
 
	if (unlikely(current_cpu->state!=CPU_STATE_RUNNING))
		return;
/*cpu_log9(current_cpu,"","b last_time %llx  current_time %llx\n",last_time,current_time);	
		 last_time=current_time;
		 current_time=get_clock();
		 cpu_log9(current_cpu,"","last_time %llx  current_time %llx\n",last_time,current_time);	
		 if (current_time==last_time)
		 	current_time=last_time+1;*/
  	struct jz4740_uart_data *d=jz4740_get_uart0(current_cpu);   
  	
  	d->output=0;
  	/*RX has high priority than TX*/
	if (vtty_is_char_avail(d->vtty))
	{
		d->lsr |= UART_LSR_DRY;
		if (d->ier & UART_IER_RDRIE)
    	{
     		 d->vm->set_irq(d->vm,d->irq);
     		 return;
    	}
		     		

	}

	if ((d->ier & UART_IER_TDRIE)&&(d->output==0)&&(d->fcr&0x10))
    {
       d->output = TRUE;
       d->vm->set_irq(d->vm,d->irq);
       return;
      }
//cpu_log9(current_cpu,"","jz4740_wdt_tcu_table[TCU_TER/4]  %x jz4740_wdt_tcu_table[TCU_TSR/4]  %x \n",jz4740_wdt_tcu_table[TCU_TER/4],jz4740_wdt_tcu_table[TCU_TSR/4] );	
if (likely(jz4740_wdt_tcu_table[TCU_TER/4]&0x01)&&(!(jz4740_wdt_tcu_table[TCU_TSR/4]&0x01)))
	{
		/*JUST TIME 0*/
		
		jz4740_wdt_tcu_table[TCU_TCNT0/4] +=( jz4740_tcu_clock[0])/1000;
		cpu_log9(current_cpu,"","jz4740_wdt_tcu_table[TCU_TCNT0/4] %x jz4740_wdt_tcu_table[TCU_TDFR0/4]  %x \n",jz4740_wdt_tcu_table[TCU_TCNT0/4],jz4740_wdt_tcu_table[TCU_TDFR0/4] );

		if (jz4740_wdt_tcu_table[TCU_TCNT0/4] >=jz4740_wdt_tcu_table[TCU_TDHR0/4] )
        {
          /*set TFR*/
          jz4740_wdt_tcu_table[TCU_TFR/4] |=1<<16;
          if (!(jz4740_wdt_tcu_table[TCU_TMR/4]&(1<<16) ))
            current_cpu->vm->set_irq(current_cpu->vm,IRQ_TCU0);
        }
	  if (jz4740_wdt_tcu_table[TCU_TCNT0/4]>=jz4740_wdt_tcu_table[TCU_TDFR0/4] )
        {
          jz4740_wdt_tcu_table[TCU_TFR/4] |=1;
          if (!(jz4740_wdt_tcu_table[TCU_TMR/4]&(0x1) ))
            {
               current_cpu->vm->set_irq(current_cpu->vm,IRQ_TCU0);
            }
           
           jz4740_wdt_tcu_table[TCU_TCNT0/4]=0;
        }
	  
	}



if (unlikely(jz4740_wdt_tcu_table[WDT_TCER/4]&0x01)&&(!(jz4740_wdt_tcu_table[TCU_TSR/4]&WDT_TIMER_STOP)))
{

    {
      jz4740_wdt_tcu_table[WDT_TCNT/4] +=jz4740_tcu_clock[0]/1000;
      if (jz4740_wdt_tcu_table[WDT_TCNT/4]&0xffff0000)
        jz4740_wdt_tcu_table[WDT_TCNT/4]=0;

     if (jz4740_wdt_tcu_table[WDT_TCNT/4]>=jz4740_wdt_tcu_table[WDT_TDR/4])
      {
        /*RESET soc*/
        current_cpu->cpu_thread_running=FALSE;
        cpu_stop(current_cpu);
        jz4740_reset(current_cpu->vm);
 
      }
    }

  
}
	

	
}

#endif


