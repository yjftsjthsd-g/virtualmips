#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<string.h>


#include "device.h"
#include "mips64_memory.h"
#include "jz4740.h"
#include "cpu.h"
#include "ptask.h"

m_uint32_t jz4740_uart_table[JZ4740_UART_NUMBER][JZ4740_UART_INDEX_MAX];
m_uint32_t jz4740_uart_uiir[JZ4740_UART_NUMBER];  //for read
m_uint32_t jz4740_uart_ufcr[JZ4740_UART_NUMBER];  //for write


/*
m_uint8_t rdtr_threshold[4]={1,4,8,15};

#define JZ4740_UART_FIFO_LEN   16
#define JZ4740_UART_FIFO_HALF_LEN   8

#define QEUEU_LEN  JZ4740_UART_FIFO_LEN


struct queue
{
   char buf[QEUEU_LEN];
   m_uint8_t read_index;
   m_uint8_t write_index;
};

static int get_queue_len(struct queue fifo)
{
      if (fifo.read_index==fifo.write_index)
        return 0;
      else if (fifo.write_index>fifo.read_index)
        return (fifo.write_index-fifo.read_index);
      else
        return (fifo.buf_len+fifo.write_index-fifo.read_index);
}
static int is_queue_full(struct queue fifo)
{
     if (fifo.read_index==(fifo.write_index+1))
      return TRUE;
     else
      return FALSE;
}

static int is_queue_empty(struct queue fifo)
{
     if (fifo.read_index==(fifo.write_index))
      return TRUE;
     else
      return FALSE;
}

static int enter_queue(struct queue fifo,char el)
{
  if (is_queue_full(fifo))
    return (-1);

  fifo[fifo.write_index]=el;
  fifo.write_index++;
  fifo.write_index = fifo.write_index%QEUEU_LEN;
  return (0);
}

static int leave_queue(struct queue fifo,char *el)
{
  if (is_queue_empty(fifo))
    return (-1);

  *el=fifo[fifo.read_index];
  fifo.read_index++;
  fifo.read_index = fifo.read_index%QEUEU_LEN;
  return (0);
}
*/


struct jz4740_uart_data {
   struct vdevice *dev;
   m_uint8_t duart_irq_seq[JZ4740_UART_NUMBER];
   //struct queue fifo[JZ4740_UART_NUMBER];
   //char  buf; 
   m_uint8_t *jz4740_uart_ptr;
   m_uint32_t jz4740_uart_size;
   vtty_t  * vtty;
   vm_instance_t *vm;
   ptask_id_t tid;

};






/* Console port input */
static void jz4740_tty_con0_input(vtty_t *vtty)
{

if (vtty_is_full(vtty))
{
  /*SET over*/
  jz4740_uart_table[0][UART_LSR/4] |= UART_LSR_OVER;
}
else
  jz4740_uart_table[0][UART_LSR/4] &= ~UART_LSR_OVER;
/*SET DRY*/
jz4740_uart_table[0][UART_LSR/4] |= UART_LSR_DRY;
printf("0");


 
}


/* Console port input */
static void  jz4740_tty_con1_input(vtty_t *vtty)
{
 
if (vtty_is_full(vtty))
{
  /*SET over*/
  jz4740_uart_table[1][UART_LSR/4] |= UART_LSR_OVER;
}
else
  jz4740_uart_table[1][UART_LSR/4] &= ~UART_LSR_OVER;
/*SET DRY*/
jz4740_uart_table[1][UART_LSR/4] |= UART_LSR_DRY;

   
}

void jz4740_uart_set_interrupt(cpu_mips_t *cpu,int channel)
{
    /*TODO : SET INTERRUPT*/
}

static int  jz4740_tty_trigger_dummy_irq(struct jz4740_uart_data *d,void *arg)
{
    int i=0;
   
    for (i=0;i<JZ4740_UART_NUMBER;i++)
    {
       if (!(jz4740_uart_ufcr[i]&UART_FCR_UME))
        {
          /*UART IS NOT ENABLED*/
          return (0);
        }
      d->duart_irq_seq[i]++;
      if (d->duart_irq_seq[i] == 2) 
        {
          if (jz4740_uart_table[i][UART_IER/4] & UART_IER_TDRIE) 
            {
              /* set transmit interrupt. I am free to transmit*/
              jz4740_uart_set_interrupt(d->vm->boot_cpu,i);
             }
          /*set TDRQ*/
         	 jz4740_uart_table[i][UART_LSR/4] |= UART_LSR_TDRQ;
	         /*set TEMP*/
	        jz4740_uart_table[i][UART_LSR/4] |= UART_LSR_TEMP;
        }

      d->duart_irq_seq[i] = 0;
   }
 return (0);
      
}
  


void *dev_jz4740_uart_access(cpu_mips_t *cpu,struct vdevice *dev,
                     m_uint32_t offset,u_int op_size,u_int op_type,
                     m_reg_t *data,m_uint8_t *has_set_value,m_uint8_t channel)
{

	struct jz4740_uart_data *d = dev->priv_data;
	
    
    
	if (offset >= d->jz4740_uart_size) {
      *data = 0;
      return NULL;
   }

	 switch (offset)
	  {
	    case UART_RBR: /*0 RBR/THR*/	
	      if (op_type == MTS_WRITE) 
	      {
	        vtty_put_char(d->vtty,(char)(*data));
	         /*set TDRQ*/
         	 jz4740_uart_table[channel][UART_LSR/4] |= UART_LSR_TDRQ;
	         /*set TEMP*/
	        jz4740_uart_table[channel][UART_LSR/4] |= UART_LSR_TEMP;
	        if (jz4740_uart_table[channel][UART_IER/4] & UART_IER_TDRIE) 
            {
              /* set transmit interrupt. I am free to transmit*/
             jz4740_uart_set_interrupt(d->vm->boot_cpu,channel);
            }
	      }
	      else if (op_type == MTS_READ)
	      {
	         if ((!(jz4740_uart_table[channel][UART_LSR/4]& UART_LSR_DRY))
	              ||(! (vtty_is_char_avail(d->vtty))))
	          {
	            /*NO DATA HAS BEEN RECEIVED*/
	            ASSERT(0,"what are you doing? No data has beed received\n");
	          }
	         else
	          {
	            *data = vtty_get_char(d->vtty);
	            if (vtty_is_char_avail(d->vtty))
	              {
	                /*SET DRY*/
	                jz4740_uart_table[channel][UART_LSR/4] |= UART_LSR_DRY;
	              }
	            else
	               jz4740_uart_table[channel][UART_LSR/4] &= ~UART_LSR_DRY;
	            if (vtty_is_full(d->vtty))
	              {
	                 /*SET over*/
	                jz4740_uart_table[channel][UART_LSR/4] |= UART_LSR_OVER;
	              }
	            else
	               jz4740_uart_table[channel][UART_LSR/4] &= ~UART_LSR_OVER;
	          }
	         
	      }
	      else 
	        assert(0);

          *has_set_value=TRUE;
          return NULL;
	      break;

	      case UART_IER: /*4 IER*/
	      case UART_LCR: /*C LCR*/
	      case UART_MCR: /*10 MCR*/
	      case UART_SPR: /*1C SPR*/
	      case UART_ISR: /*20 ISR*/
	      case UART_UMR: /*24 UMR*/
	      case UART_UACR: /*28 UACR*/
	        return((void *)(d->jz4740_uart_ptr + offset));
	        break;
	      case UART_IIR: /*8 IiR*/
	          if (op_type == MTS_READ)
	          { 
	              /*READ UIIR*/
	              return((void *)(&jz4740_uart_uiir[0]));
	          }
	          else if (op_type == MTS_WRITE) /*write fcr*/
	            return((void *)(&jz4740_uart_ufcr[0]));
	          break;
	      case UART_LSR: /*14 LSR*/
	      case UART_MSR: /*18 MSR*/
	        {
	           ASSERT(op_type==MTS_READ,"Write to read only register of UART. offset %x\n",offset);
	           return((void *)(d->jz4740_uart_ptr + offset));
	        }
	      break;

	      default:
	        ASSERT(0,"Error offset of UART. offset %x\n",offset);
	        
      
          #if 0
	      if (op_type == MTS_WRITE) 
	      {
         	 /*if FIFO model, write to FIFO Buffer*/
         	 if (jz4740_uart_ufcr[channel]&UART_FCR_FME)
         	  {
         	    enter_queue(d->fifo[channel], (char)(*data));
         	    if (get_queue_len(d->fifo[channel])>=JZ4740_UART_FIFO_HALF_LEN)
         	      {
         	        /*clear TDRQ*/
         	        jz4740_uart_table[channel][UART_LSR] &= ~UART_LSR_TDRQ;
         	             	         
         	      }
         	    else
         	      {
         	       /*set TDRQ*/
         	        jz4740_uart_table[channel][UART_LSR] |= UART_LSR_TDRQ;
         	       if (jz4740_uart_table[channel][UART_IER] & UART_IER_TDRIE) 
                    {
                        /* set transmit interrupt. I am free to transmit*/
                      JZ4740_uart_set_interrupt(d->vm->boot_cpu,i);
                    }

         	       
         	      }
         	  }
         	 else
         	  {
         	    /*NON FIFO*/
         	    d->buf=(char)(*data);
         	    /*clear TDRQ*/
         	    jz4740_uart_table[channel][UART_LSR] &= ~UART_LSR_TDRQ;
         	  }
         	  
         	  
          }
	      #endif

	  }
 
   return(NULL);
   
}


void dev_jz4740_uart_init_defaultvalue(int uart_index)
{
  int i;
  for (i=0;i<JZ4740_UART_NUMBER;i++)
  {
    jz4740_uart_uiir[i]=0x1;
    jz4740_uart_ufcr[i]=0x0;
    /*set
        LSR(TDRQ)=1: FREE TO TRANSMIT
        LSR(DRY)=0:  NO DATA has been received
    */
    jz4740_uart_table[i][UART_LSR/4] |= UART_LSR_TDRQ;
  }

}

void *dev_jz4740_uart_access0(cpu_mips_t *cpu,struct vdevice *dev,
                     m_uint32_t offset,u_int op_size,u_int op_type,
                     m_reg_t *data,m_uint8_t *has_set_value)
{
   return dev_jz4740_uart_access(cpu,dev,offset,op_size,op_type,data,has_set_value,0);
}

void *dev_jz4740_uart_access1(cpu_mips_t *cpu,struct vdevice *dev,
                     m_uint32_t offset,u_int op_size,u_int op_type,
                     m_reg_t *data,m_uint8_t *has_set_value)
{
   return dev_jz4740_uart_access(cpu,dev,offset,op_size,op_type,data,has_set_value,1);
}

int dev_jz4740_uart_init(vm_instance_t *vm,char *name,m_pa_t paddr,m_uint32_t len,vtty_t *vtty,int uart_index)
{
 	struct jz4740_uart_data *d;

   /* allocate the private data structure */
   if (!(d = malloc(sizeof(*d)))) {
      fprintf(stderr,"JZ4740 UART: unable to create device.\n");
      return  (-1);
   }
   memset(d,0,sizeof(*d));
   if (!(d->dev = dev_create(name)))
     goto err_dev_create;
   d->dev->priv_data = d;
   d->dev->phys_addr = paddr;
   d->dev->phys_len = len;
   d->dev->flags     = VDEVICE_FLAG_NO_MTS_MMAP;
   d->vm=vm;
   (*d).vtty=vtty;
   d->jz4740_uart_size = len;
   if (uart_index==0)
    {
      d->dev->handler   = dev_jz4740_uart_access0;
      (*d).vtty->read_notifier = jz4740_tty_con0_input;
   }
   else   if (uart_index==1)
    {
      d->dev->handler   = dev_jz4740_uart_access1;
      (*d).vtty->read_notifier = jz4740_tty_con1_input;
    }
   else 
    {
      ASSERT(0,"We only emulate 2 UART in JZ4740\n");
    }

    d->jz4740_uart_ptr = (m_uint8_t*)(m_iptr_t)(&jz4740_uart_table[uart_index]);
      
	vm_bind_device(vm,d->dev);
	dev_jz4740_uart_init_defaultvalue(uart_index);

   d->tid = ptask_add((ptask_callback)jz4740_tty_trigger_dummy_irq,d,NULL);
	return  (0);

   err_dev_create:
   		free(d);
   		return  (-1);
}


   


