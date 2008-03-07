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



 m_uint32_t jz4740_wdt_tcu_table[JZ4740_WDT_INDEX_MAX];

struct jz4740_wdt_tcu_data {
   struct vdevice *dev;
   m_uint8_t *jz4740_wdt_tcu_ptr;
   m_uint32_t jz4740_wdt_tcu_size;
};

m_uint32_t   past_instructions=0;
#define COUNT_PER_INSTRUCTION   0X5
/*JUST TIMER 0*/
void forced_inline virtual_jz4740_timer(cpu_mips_t *cpu)
{
  
  if (jz4740_wdt_tcu_table[TCU_TSR/4]&0x01)
    {
    
    return;
    }
if (jz4740_wdt_tcu_table[TCU_TER/4]&0x01)
{
  //allow counter
  past_instructions++;
  if (past_instructions==COUNT_PER_INSTRUCTION)
    {
      jz4740_wdt_tcu_table[TCU_TCNT0/4] +=1 ;
      cpu_log(cpu,"","jz4740_wdt_tcu_table[TCU_TCNT0/4]  %x\n",jz4740_wdt_tcu_table[TCU_TCNT0/4] );
      /*TODO: INTERRUPT*/
    }
}

  
}

void *dev_jz4740_wdt_tcu_access(cpu_mips_t *cpu,struct vdevice *dev,
                     m_uint32_t offset,u_int op_size,u_int op_type,
                     m_reg_t *data,m_uint8_t *has_set_value)
{

	struct jz4740_wdt_tcu_data *d = dev->priv_data;
	m_uint32_t mask_data,mask;
	if (offset >= d->jz4740_wdt_tcu_size) {
      *data = 0;
      return NULL;
   }

if (op_type==MTS_WRITE)
	  {
        ASSERT(offset!=TCU_TSR,"Write to read only register in TCU. offset %x\n",offset);
         ASSERT(offset!=TCU_TER,"Write to read only register in TCU. offset %x\n",offset);
         ASSERT(offset!=TCU_TFR,"Write to read only register in TCU. offset %x\n",offset);
         ASSERT(offset!=TCU_TMR,"Write to read only register in TCU. offset %x\n",offset);
}
else if (op_type==MTS_READ)
{
   ASSERT(offset!=TCU_TSSR,"Read write only register in TCU. offset %x\n",offset);
   ASSERT(offset!=TCU_TSCR,"Read write only register in TCU. offset %x\n",offset);
   ASSERT(offset!=TCU_TESR,"Read write only register in TCU. offset %x\n",offset);
   ASSERT(offset!=TCU_TECR,"Read write only register in TCU. offset %x\n",offset);
   ASSERT(offset!=TCU_TFSR,"Read write only register in TCU. offset %x\n",offset);
   ASSERT(offset!=TCU_TFCR,"Read write only register in TCU. offset %x\n",offset);
   ASSERT(offset!=TCU_TMSR,"Read write only register in TCU. offset %x\n",offset);
   ASSERT(offset!=TCU_TMCR,"Read write only register in TCU. offset %x\n",offset);
}
   switch(offset)
    {
     

      case TCU_TSSR:  /*set*/
          printf("I AM asdf\n");
          assert (op_type==MTS_WRITE);
          mask = (1<<(op_size*8))-1;
	      mask_data = (*data)&mask;
	      jz4740_wdt_tcu_table[TCU_TSR/4] |= mask_data;
	      *has_set_value=TRUE;
	      break;
	  case TCU_TSCR: /*clear*/
          assert (op_type==MTS_WRITE);
          mask = (1<<(op_size*8))-1;
	      mask_data = (*data)&mask;
	      mask_data= ~(mask_data);
	       jz4740_wdt_tcu_table[TCU_TSR/4]  &= mask_data;
	       *has_set_value=TRUE;
	      break;

	       case TCU_TESR:  /*set*/
          assert (op_type==MTS_WRITE);
          mask = (1<<(op_size*8))-1;
	      mask_data = (*data)&mask;
	      jz4740_wdt_tcu_table[TCU_TER/4] |= mask_data;
	      *has_set_value=TRUE;
	      break;
	  case TCU_TECR: /*clear*/
          assert (op_type==MTS_WRITE);
          mask = (1<<(op_size*8))-1;
	      mask_data = (*data)&mask;
	      mask_data= ~(mask_data);
	       jz4740_wdt_tcu_table[TCU_TER/4]  &= mask_data;
	       *has_set_value=TRUE;
	      break;
      
             case TCU_TFSR:  /*set*/
          assert (op_type==MTS_WRITE);
          mask = (1<<(op_size*8))-1;
	      mask_data = (*data)&mask;
	      jz4740_wdt_tcu_table[TCU_TFR/4] |= mask_data;
	      *has_set_value=TRUE;
	      break;
	  case TCU_TFCR: /*clear*/
          assert (op_type==MTS_WRITE);
          mask = (1<<(op_size*8))-1;
	      mask_data = (*data)&mask;
	      mask_data= ~(mask_data);
	       jz4740_wdt_tcu_table[TCU_TFR/4]  &= mask_data;
	       *has_set_value=TRUE;
	      break;

      case TCU_TMSR:  /*set*/
          assert (op_type==MTS_WRITE);
          mask = (1<<(op_size*8))-1;
	      mask_data = (*data)&mask;
	      jz4740_wdt_tcu_table[TCU_TMR/4] |= mask_data;
	      *has_set_value=TRUE;
	      break;
	  case TCU_TMCR: /*clear*/
          assert (op_type==MTS_WRITE);
          mask = (1<<(op_size*8))-1;
	      mask_data = (*data)&mask;
	      mask_data= ~(mask_data);
	       jz4740_wdt_tcu_table[TCU_TMR/4]  &= mask_data;
	       *has_set_value=TRUE;
	      break;
	  default:
        return((void *)(d->jz4740_wdt_tcu_ptr + offset));
        
    }
	
  return NULL;

}

void dev_jz4740_wdt_tcu_init_defaultvalue()
{

/*set TCU_TSR=0xffffffff*/
jz4740_wdt_tcu_table[TCU_TSR/4]=0XFFFFFFFF;


}
int dev_jz4740_wdt_tcu_init(vm_instance_t *vm,char *name,m_pa_t paddr,m_uint32_t len)
{
 	struct jz4740_wdt_tcu_data *d;

   /* allocate the private data structure */
   if (!(d = malloc(sizeof(*d)))) {
      fprintf(stderr,"jz4740_wdt_tcu: unable to create device.\n");
      return  (-1);
   }

   memset(d,0,sizeof(*d));
   if (!(d->dev = dev_create(name)))
     goto err_dev_create;
   d->dev->priv_data = d;
   d->dev->phys_addr = paddr;
   d->dev->phys_len = len;
   d->jz4740_wdt_tcu_ptr = (m_uint8_t*)(&jz4740_wdt_tcu_table[0]);
   d->jz4740_wdt_tcu_size = len;
   d->dev->handler   = dev_jz4740_wdt_tcu_access;
   d->dev->flags     = VDEVICE_FLAG_NO_MTS_MMAP;
   
	vm_bind_device(vm,d->dev);
	dev_jz4740_wdt_tcu_init_defaultvalue();
	
	return  (0);

   err_dev_create:
   		free(d);
   		return  (-1);
}


   


   

