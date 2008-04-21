 /*
 * Copyright (C) yajin 2008 <yajinzhou@gmail.com >
 *     
 * This file is part of the virtualmips distribution. 
 * See LICENSE file for terms of the license. 
 *
 */


 

/*RTC. */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<string.h>


#include "device.h"
#include "mips64_memory.h"
#include "cpu.h"
#include "jz4740.h"
#include "vp_timer.h"
#define RTC_TIMEOUT  1000  //1000MS=1S 
extern cpu_mips_t *current_cpu;
 m_uint32_t jz4740_rtc_table[JZ4740_RTC_INDEX_MAX];

struct jz4740_rtc_data {
   struct vdevice *dev;
   m_uint8_t *jz4740_rtc_ptr;
   m_uint32_t jz4740_rtc_size;
   vp_timer_t *rtc_timer;

};

void *dev_jz4740_rtc_access(cpu_mips_t *cpu,struct vdevice *dev,
                     m_uint32_t offset,u_int op_size,u_int op_type,
                     m_reg_t *data,m_uint8_t *has_set_value)
{

	struct jz4740_rtc_data *d = dev->priv_data;
	if (offset >= d->jz4740_rtc_size) {
      *data = 0;
      return NULL;
   }
	cpu_log16(cpu,"","offset %x op_type %x *data %x \n",offset,op_type,*data);

	switch (offset)
	{
		case RTC_RCR:
			if (op_type==MTS_READ)
			{
				/*RTC_RCR (RTC_RCR_WRDY )=1 bit 7*/
				jz4740_rtc_table[RTC_RCR/4] |= RTC_RCR_WRDY;
			}
			else if (op_type==MTS_WRITE)
			{
				if (*data &RTC_RCR_RTCE)
				{
					dev_jz4740_active_rtc(d);
					cpu_log16(cpu,"","active rtc\n");
				}
				else
				{
					dev_jz4740_unactive_rtc(d);
				}
			}
		break;
		
	}

  if (offset==0x4)
  	{
  		cpu_log16(cpu,"","offset %x *data %x \n",offset,jz4740_rtc_table[RTC_RSR/4] );
  	}
  return((void *)(d->jz4740_rtc_ptr + offset));
  

}

void dev_jz4740_rtc_init_defaultvalue()
{
memset(jz4740_rtc_table,0x0,sizeof(jz4740_rtc_table));
    
}

void dev_jz4740_rtc_reset(cpu_mips_t *cpu,struct vdevice *dev)
{
  dev_jz4740_rtc_init_defaultvalue();
}

void dev_jz4740_active_rtc(struct jz4740_rtc_data *d)
{
	vp_mod_timer(d->rtc_timer, vp_get_clock(rt_clock)+RTC_TIMEOUT);
}

void dev_jz4740_unactive_rtc(struct jz4740_rtc_data *d)
{
	 vp_del_timer(d->rtc_timer);
}

void dev_jz4740_rtc_cb(void *opaque)
{
	struct jz4740_rtc_data *d=opaque;

	if (jz4740_rtc_table[RTC_RCR/4] &RTC_RCR_RTCE)
	{
		//rtc enable
		jz4740_rtc_table[RTC_RCR/4] |= RTC_RCR_1HZ;
		if (jz4740_rtc_table[RTC_RCR/4] &RTC_RCR_1HZIE)
		{
			cpu_log16(current_cpu,"","IRQ_RTC1\n");
			current_cpu->vm->set_irq(current_cpu->vm,IRQ_RTC);
		}

		jz4740_rtc_table[RTC_RSR/4] ++;
		if (jz4740_rtc_table[RTC_RSR/4]==jz4740_rtc_table[RTC_RSAR/4] )
		{
			if (jz4740_rtc_table[RTC_RCR/4] &RTC_RCR_AE)
				{
			jz4740_rtc_table[RTC_RCR/4] |= RTC_RCR_AF;
			if (jz4740_rtc_table[RTC_RCR/4] &RTC_RCR_AIE)
			{
				cpu_log16(current_cpu,"","IRQ_RTC2\n");
				current_cpu->vm->set_irq(current_cpu->vm,IRQ_RTC);
			}
			}
		}
	}
	dev_jz4740_active_rtc(d);
		
}


int dev_jz4740_rtc_init(vm_instance_t *vm,char *name,m_pa_t paddr,m_uint32_t len)
{
 	struct jz4740_rtc_data *d;

   /* allocate the private data structure */
   if (!(d = malloc(sizeof(*d)))) {
      fprintf(stderr,"jz4740_rtc: unable to create device.\n");
      return  (-1);
   }

   memset(d,0,sizeof(*d));
   if (!(d->dev = dev_create(name)))
     goto err_dev_create;
   d->dev->priv_data = d;
   d->dev->phys_addr = paddr;
   d->dev->phys_len = len;
   d->jz4740_rtc_ptr = (m_uint8_t*)(&jz4740_rtc_table[0]);
   d->jz4740_rtc_size = len;
   d->dev->handler   = dev_jz4740_rtc_access;
    d->dev->reset_handler   = dev_jz4740_rtc_reset;
   d->dev->flags     = VDEVICE_FLAG_NO_MTS_MMAP;
   d->rtc_timer=vp_new_timer(rt_clock, dev_jz4740_rtc_cb, d);
   
	vm_bind_device(vm,d->dev);
	dev_jz4740_rtc_init_defaultvalue();
	
	return  (0);

   err_dev_create:
   		free(d);
   		return  (-1);
}


   


   

