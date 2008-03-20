 /*
 * Copyright (C) yajin 2008 <yajinzhou@gmail.com >
 *     
 * This file is part of the virtualmips distribution. 
 * See LICENSE file for terms of the license. 
 *
 */

 /*JZ4740 UART Emulation.
 TODO:uart interrupt
 */

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

#if 0
/* Interrupt Enable Register (IER) */
#define	IER_ERXRDY    0x1
#define	IER_ETXRDY    0x2

/* Interrupt Identification Register */
#define IIR_NPENDING  0x01   /* 0: irq pending, 1: no irq pending */
#define	IIR_TXRDY     0x02
#define	IIR_RXRDY     0x04

/* Line Status Register (LSR) */
#define	LSR_RXRDY     0x01
#define	LSR_TXRDY     0x20
#define	LSR_TXEMPTY   0x40

/* UART channel */
struct dev16c550_channel {
   u_int ier,output;
   m_uint32_t lsr;     //line status
   m_uint32_t mcr;   //mcr
   vtty_t *vtty;
};

/* 16c550  structure */
struct dev16c550_data {
   struct vdevice *dev;
   vm_instance_t *vm;
   //u_int irq;
   u_int irq[2];
   
   /* Register offset divisor */
   u_int reg_div;

   /* Periodic task to trigger DUART IRQ */
   ptask_id_t tid;
   
   struct dev16c550_channel channel[2];
   //struct dev16c550_channel channel;
   u_int duart_irq_seq;
};



/* Console port input */
static void tty_con1_input(vtty_t *vtty)
{
   struct dev16c550_data *d = vtty->priv_data;

   if (d->channel[0].ier & IER_ERXRDY)
      d->vm->set_irq(d->vm,d->irq[0]);
}

/* AUX port input */
static void tty_con2_input(vtty_t *vtty)
{
   struct dev16c550_data *d = vtty->priv_data;

   if (d->channel[1].ier & IER_ERXRDY)
      d->vm->set_irq(d->vm,d->irq[1]);
}


/* IRQ trickery for Console and AUX ports */
static int tty_trigger_dummy_irq(struct dev16c550_data *d,void *arg)
{
   d->duart_irq_seq++;
  // cpu_log(d->vm->cpu,"","tty_trigger_dummy_irq\n");
   if (d->duart_irq_seq == 2) {
   			//cpu_log(d->vm->cpu,"","d->channel[0].ier %x d->channel[0].ier & IER_ETXRDY %x\n",d->channel[0].ier ,d->channel[0].ier & IER_ETXRDY);
      if (d->channel[0].ier & IER_ETXRDY) {
      	  //cpu_log(d->vm->cpu,"","IRQ  I AM READY TO TRANSMIT\n");
         d->channel[0].output = TRUE;
         d->vm->set_irq(d->vm,d->irq[0]);
      }
      if (d->channel[1].ier & IER_ETXRDY) {
         d->channel[1].output = TRUE;
         d->vm->set_irq(d->vm,d->irq[1]);
      }


      d->duart_irq_seq = 0;
   }

   return(0);
}

m_uint32_t dummy;
#define  DEBUG_ACCESS  1
//m_uint32_t  rtl865x_uart_msr=0x00000005; //big end  0x05000000
void* dev16c550_access(cpu_mips_t * cpu,struct vdevice *dev,
                     m_uint32_t offset,u_int op_size,u_int op_type,
                     m_uint32_t *data,m_uint8_t *has_set_value)
{
	
   struct dev16c550_data *d = dev->priv_data;
   int channel = 0;
   u_char odata;
   
    if (op_type == MTS_READ)
      *data = 0;

#if DEBUG_ACCESS
   if (op_type == MTS_READ) {
     // cpu_log(cpu,"NS16552","read from 0x%x, pc=0x%x\n",
     //         offset,cpu->pc);
   } else {
     // cpu_log(cpu,"NS16552","write to 0x%x, value=0x%x, pc=0x%x\n",
     //         offset,*data,cpu->pc);
   }
#endif

 

   if (offset >= 0x1000)
      channel = 1;

   switch(offset) {
      /* Receiver Buffer Reg. (RBR) / Transmitting Holding Reg. (THR) */
      case 0x00:
      case 0x1000:
         if (op_type == MTS_WRITE) {
         	 vtty_put_char(d->channel[channel].vtty,(char)(*data));

            if (d->channel[channel].ier & IER_ETXRDY)
               d->vm->set_irq(d->vm,d->irq[channel]);
            d->channel[channel].output = TRUE;
            
         } else {
            *data = vtty_get_char(d->channel[channel].vtty);
         }
          *has_set_value =TRUE;
         break;

      /* Interrupt Enable Register (IER) */
      case 0x04:
      case 0x1004:
         if (op_type == MTS_READ) {
            *data = d->channel[channel].ier;
         } else {
            d->channel[channel].ier = *data & 0xFF;
             //cpu_log(cpu,"","ACCESS IER\n");
             //cpu_log(cpu,"","*data %x,offset %x\n",*data,offset);
            if ((*data & 0x02) == 0) {   /* transmit holding register */
               d->channel[channel].vtty->managed_flush = TRUE;
               vtty_flush(d->channel[channel].vtty);               
            }
         }
           *has_set_value =TRUE;
         break;

      /* Interrupt Ident Register (IIR) */
      case 0x08:
      case 0x1008:
         d->vm->clear_irq(d->vm,d->irq[channel]);
   	        if (op_type == MTS_READ) {
            odata = IIR_NPENDING;

            if (vtty_is_char_avail(d->channel[channel].vtty)) {
               odata = IIR_RXRDY;
            } else {
                //cpu_log(cpu,"","*d->channel[channel].output %x\n",d->channel[channel].output);
               if (d->channel[channel].output) {
                  odata = IIR_TXRDY;
                  //cpu_log(cpu,"","IIR_TXRDY\n");
                  d->channel[channel].output = 0;
               }
            }

            *data = odata;
         }
   	       //cpu_log(cpu,"","before return\n");
   	       *has_set_value =TRUE;
         break;

          /* Line status Register (LSR ) */
      case 0x0c:
      case 0x100c:
      	return &d->channel[channel].lsr;
      	 	
      	  
        // return SPECIAL_HADDR;
         break;
         
      /* Modem control */
      case 0x10:
      case 0x1010:
      	return &d->channel[channel].mcr;
        // return SPECIAL_HADDR;
         break;

      /* Line Status Register (LSR) */
      case 0x14:
      case 0x1014:
         if (op_type == MTS_READ) {
            odata = 0;

            if (vtty_is_char_avail(d->channel[channel].vtty))
               odata |= LSR_RXRDY;

            odata |= LSR_TXRDY|LSR_TXEMPTY;
            *data = odata;
         }
           *has_set_value =TRUE;
         break;
      case 0x18:
      case 0x1018:
       *data = htovm32(0x00000005);
      	*has_set_value =TRUE;
     break;
     case 0x1c:
     case 0x101c:
     case 0x20:
     case 0x1020:
        return &dummy;
#if DEBUG_UNKNOWN
      default:
         if (op_type == MTS_READ) {
            //cpu_log(cpu,"NS16552","read from addr 0x%x, pc=0x%llx (size=%u)\n",
            //        offset,cpu_get_pc(cpu),op_size);
         } else {
            cpu_log(cpu,
                    "NS16552","write to addr 0x%x, value=0x%llx, "
                    "pc=0x%llx (size=%u)\n",
                    offset,*data,cpu_get_pc(cpu),op_size);
         }
#endif
   }

   return NULL;
	 
	//return NULL;
}

void dev_jz4740_uart_init_defaultvalue(int uart_index)
{
  int i;
  for (i=0;i<JZ4740_UART_NUMBER;i++)
  {
  //  jz4740_uart_uiir[i]=0x1;
  //  jz4740_uart_ufcr[i]=0x0;
    /*set
        LSR(TDRQ)=1: FREE TO TRANSMIT
        LSR(DRY)=0:  NO DATA has been received
    */
  //  jz4740_uart_table[i][UART_LSR/4] |= UART_LSR_TDRQ;
  }

}


void dev_jz4740_uart_reset(cpu_mips_t *cpu,struct vdevice *dev,int uart_index)
{
  dev_jz4740_uart_init_defaultvalue(uart_index);
}

void dev_jz4740_uart_reset0(cpu_mips_t *cpu,struct vdevice *dev)
{
  dev_jz4740_uart_init_defaultvalue(0);
}
void dev_jz4740_uart_reset1(cpu_mips_t *cpu,struct vdevice *dev)
{
  dev_jz4740_uart_init_defaultvalue(1);
}

int dev_jz4740_uart_init(vm_instance_t *vm,m_uint32_t paddr,m_uint32_t len,
                     u_int irq0,u_int irq1,vtty_t *vtty_A,vtty_t *vtty_B)
{  
   struct dev16c550_data *d;
 
   /* Allocate private data structure */
   if (!(d = malloc(sizeof(*d)))) {
      fprintf(stderr,"ipm16c550: out of memory\n");
      return(-1);
   }

   memset(d,0,sizeof(*d));
  

   
   d->vm  = vm;
   d->irq[0] = irq0;
   d->irq[1] = irq1;
   //d->reg_div = reg_div;
   
    d->channel[0].vtty = vtty_A;
    d->channel[1].vtty = vtty_A;



    if (!(d->dev = dev_create("dev16c550")))
     goto err_dev_create;
   d->dev->priv_data = d;
   d->dev->phys_addr = paddr;
   d->dev->phys_len = len;
   d->dev->handler   = dev16c550_access;
	
    d->dev->flags |= VDEVICE_FLAG_NO_MTS_MMAP;
	vtty_A->priv_data = d;
   vtty_B->priv_data = d;
  // if (paddr==RTL8389_UART0_BASE)
    vtty_A->read_notifier = tty_con1_input;
  // else if (paddr==RTL8389_UART1_BASE)
     vtty_B->read_notifier = tty_con2_input;
   //else 
   // assert(0);
   d->dev->reset_handler   = dev_jz4740_uart_reset0;

   /* Trigger periodically a dummy IRQ to flush buffers */
   d->tid = ptask_add((ptask_callback)tty_trigger_dummy_irq,d,NULL);


  vm_bind_device(vm,d->dev);
    
      		 
   return(TRUE);

    err_dev_create:
   		free(d);
   		return FALSE;
}


#endif



m_uint32_t jz4740_uart_table[JZ4740_UART_NUMBER][JZ4740_UART_INDEX_MAX];
m_uint32_t jz4740_uart_uiir[JZ4740_UART_NUMBER];  //for read
m_uint32_t jz4740_uart_ufcr[JZ4740_UART_NUMBER];  //for write





struct jz4740_uart_data {
   struct vdevice *dev;
   m_uint8_t duart_irq_seq[JZ4740_UART_NUMBER];
   m_uint8_t *jz4740_uart_ptr;
   m_uint32_t jz4740_uart_size;
   vtty_t  * vtty;
   vm_instance_t *vm;
   ptask_id_t tid;

};


void jz4740_uart_set_interrupt(cpu_mips_t *cpu,int channel)
{
    /*clear uiir.inpend*/
    //jz4740_uart_table[channel][UART_IIR/4] &= ~0x1;
    if (channel==0)
      {
        cpu_log1(cpu,"","set uart irq \n");
        cpu->vm->set_irq(cpu->vm,IRQ_UART0);
      }
}



/* Console port input */
static void jz4740_tty_con0_input(vtty_t *vtty)
{


  

/*SET DRY*/


//printf("jz4740_uart_table[0][UART_IER/4] %x\n",jz4740_uart_table[0][UART_IER/4]);
/*if interrupt enable, generate interrupt*/
if (jz4740_uart_table[0][UART_IER/4] & UART_IER_RDRIE) 
{
 //if (jz4740_uart_uiir[0]&0x1)
  {
  /* set transmit interrupt. I have received a data*/
 jz4740_uart_set_interrupt(vtty->vm->boot_cpu,0);
  }
 
 }

 
}


/* Console port input */
static void  jz4740_tty_con1_input(vtty_t *vtty)
{
 
if (vtty_is_char_avail(vtty)&&(vtty_is_full(vtty)))
{
  /*SET over*/
  jz4740_uart_table[1][UART_LSR/4] |= UART_LSR_OVER;
}
else
  jz4740_uart_table[1][UART_LSR/4] &= ~UART_LSR_OVER;
/*SET DRY*/
jz4740_uart_table[1][UART_LSR/4] |= UART_LSR_DRY;
/*if interrupt enable, generate interrupt*/
  if (jz4740_uart_table[0][UART_IER/4] & UART_IER_RDRIE) 
{
  /* set transmit interrupt. I have received a data*/
 //jz4740_uart_set_interrupt(vtty->vm->boot_cpu,0);
 }
   
}

int want_tran=0;

static int  jz4740_tty_trigger_dummy_irq(struct jz4740_uart_data *d,void *arg)
{
    int i=0;
   
    //for (i=0;i<JZ4740_UART_NUMBER;i++)
    {
     //  if (!(jz4740_uart_ufcr[i]&UART_FCR_UME))
       // {
          /*UART IS NOT ENABLED*/
        //  return (0);
       // }
      d->duart_irq_seq[i]++;
     if (d->duart_irq_seq[i] == 2) 
        {
         //cpu_log1(d->vm->boot_cpu,"","jz4740_uart_table[i][UART_IER/4] %x\n",jz4740_uart_table[i][UART_IER/4]);
          if (jz4740_uart_table[i][UART_IER/4] & UART_IER_TDRIE) 
            {
              
              /* set transmit interrupt. I am free to transmit*/
             // if (want_tran)
                {
                jz4740_uart_set_interrupt(d->vm->boot_cpu,i);
              //jz4740_uart_uiir[i] &= 0XFFFFFFF0;
              //jz4740_uart_uiir[i]+= 0X2;
             want_tran=1;
              //cpu_log1(d->vm->boot_cpu,"","want_tran %x\n",want_tran);
                /*set TDRQ*/
         	 jz4740_uart_table[i][UART_LSR/4] |= UART_LSR_TDRQ;
	         /*set TEMP*/
	        jz4740_uart_table[i][UART_LSR/4] |= UART_LSR_TEMP;
                }
                
             }
         d->duart_irq_seq[i] = 0;
        }

    
   }
 return (0);
      
}
  


void *dev_jz4740_uart_access(cpu_mips_t *cpu,struct vdevice *dev,
                     m_uint32_t offset,u_int op_size,u_int op_type,
                     m_reg_t *data,m_uint8_t *has_set_value,m_uint8_t channel)
{

	struct jz4740_uart_data *d = dev->priv_data;
	
    u_char odata;
    
	if (offset >= d->jz4740_uart_size) {
      *data = 0;
      return NULL;
   }

	cpu_log2(cpu,"","offset %x type %x *data %x\n",offset,op_type,*data);

	 switch (offset)
	  {
	    case UART_RBR: /*0 RBR/THR*/	
	      if (op_type == MTS_WRITE) 
	      {
	        vtty_put_char(d->vtty,(char)(*data));
	         cpu_log2(cpu,"","writing data %c \n",*data);
	         
	         /*set TDRQ*/
         	 jz4740_uart_table[channel][UART_LSR/4] |= UART_LSR_TDRQ;
	         /*set TEMP*/
	        jz4740_uart_table[channel][UART_LSR/4] |= UART_LSR_TEMP;

	         

	         
	        
	      }
	      else if (op_type == MTS_READ)
	      {
	         if ((!(jz4740_uart_table[channel][UART_LSR/4]& UART_LSR_DRY))
	             ||(! (vtty_is_char_avail(d->vtty)))
	              )

	        {
	            /*NO DATA HAS BEEN RECEIVED*/
	            //ASSERT(0,"what are you doing? No data has beed received\n");
	            *data = 0;
	          }
	        else
	          {
	            *data = vtty_get_char(d->vtty);
	            cpu_log2(cpu,"","reading data %c \n",*data);
	            if (vtty_is_char_avail(d->vtty))
	              {
	                /*SET DRY*/
	                jz4740_uart_table[channel][UART_LSR/4] |= UART_LSR_DRY;
	              }
	            else
	              {
	                 jz4740_uart_table[channel][UART_LSR/4] &= ~UART_LSR_DRY;
	                 //cpu_log1(cpu,"","seting uiir  %x\n",jz4740_uart_uiir[channel]);
	              }
	              
	          }
	         
	      }
	      else 
	        assert(0);

	      
//want_tran = 0;
	         if (jz4740_uart_table[channel][UART_IER/4] & UART_IER_TDRIE) 
            {
              //cpu_log1(d->vm->boot_cpu,"","22TDRIE ENABLE jz4740_uart_table[i][UART_IER/4] %x",jz4740_uart_table[channel][UART_IER/4]);
              /* set transmit interrupt. I am free to transmit*/
                jz4740_uart_set_interrupt(d->vm->boot_cpu,channel);
              want_tran=1;
             // jz4740_uart_uiir[channel] &= 0XFFFFFFF0;
              //jz4740_uart_uiir[channel]+= 0X2;
                /*set TDRQ*/
         	 jz4740_uart_table[channel][UART_LSR/4] |= UART_LSR_TDRQ;
	         /*set TEMP*/
	        jz4740_uart_table[channel][UART_LSR/4] |= UART_LSR_TEMP;
              
            }

	      

          *has_set_value=TRUE;
          return NULL;
	      break;

	      case UART_IER: /*4 IER*/
	      if ((op_type==MTS_WRITE))
	          cpu_log1(cpu,"","wite IER %x \n",*data);
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
	          //*data=1;
	           //if (vtty_is_char_avail(d->vtty)) {
	           // *data=0X400;
	           // }
	              /*READ UIIR*/
	              //*has_set_value=TRUE;

	          odata = 1;

            if (vtty_is_char_avail(d->vtty)) {
              odata = 4;
            } else {
               if (want_tran)
                { 
                  odata = 2;
                  want_tran=0;
                }
                

              }
            cpu_log1(d->vm->boot_cpu,"","odata %x\n",odata);
            *data=odata;
            *has_set_value=TRUE;
            return NULL;
	             /*  cpu_log1(cpu,"","read jz4740_uart_uiir %x\n",jz4740_uart_uiir[channel]);
	              if (want_tran)
	                {
	                  *data=0x2;
	                }
	              else
	                {
	                  want_tran=0;
	                  jz4740_uart_uiir[channel] |= 0x1;
	                 jz4740_uart_uiir[channel] &= 0xfffffff1;
	                 return((void *)(&jz4740_uart_uiir[channel]));
	                }*/
	             
	          }
	          else if (op_type == MTS_WRITE) /*write fcr*/
	            {
	            cpu_log1(cpu,"","write  jz4740_uart_ufcr *data %x\n",*data );
	            return((void *)(&jz4740_uart_ufcr[channel]));
	            }
	            
	          break;
	      case UART_LSR: /*14 LSR*/
	        cpu_log1(cpu,"","lsr %x\n",jz4740_uart_table[channel][UART_LSR/4]);
	        if (op_type == MTS_READ) {
            odata = 0;

            if (vtty_is_char_avail(d->vtty))
               odata |= 1;

            odata |= 0x20|0x40;
            *data = odata;
         }
           *has_set_value =TRUE;
         break;
         
	      case UART_MSR: /*18 MSR*/
	        {
	           ASSERT(op_type==MTS_READ,"Write to read only register of UART. offset %x\n",offset);
	           return((void *)(d->jz4740_uart_ptr + offset));
	        }
	      break;

	      default:
	        ASSERT(0,"Error offset of UART. offset %x\n",offset);
	        
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


void dev_jz4740_uart_reset(cpu_mips_t *cpu,struct vdevice *dev,int uart_index)
{
  dev_jz4740_uart_init_defaultvalue(uart_index);
}

void dev_jz4740_uart_reset0(cpu_mips_t *cpu,struct vdevice *dev)
{
  dev_jz4740_uart_init_defaultvalue(0);
}
void dev_jz4740_uart_reset1(cpu_mips_t *cpu,struct vdevice *dev)
{
  dev_jz4740_uart_init_defaultvalue(1);
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
      d->dev->reset_handler   = dev_jz4740_uart_reset0;
      (*d).vtty->read_notifier = jz4740_tty_con0_input;
   }
   else   if (uart_index==1)
    {
      d->dev->handler   = dev_jz4740_uart_access1;
       d->dev->reset_handler   = dev_jz4740_uart_reset1;
      (*d).vtty->read_notifier = jz4740_tty_con1_input;
    }
   else 
    {
      ASSERT(0,"We only emulate 2 UART in JZ4740\n");
    }

   

    d->jz4740_uart_ptr = (m_uint8_t*)(m_iptr_t)(&jz4740_uart_table[uart_index]);
      
	vm_bind_device(vm,d->dev);
	//dev_jz4740_uart_init_defaultvalue(uart_index);
	

   d->tid = ptask_add((ptask_callback)jz4740_tty_trigger_dummy_irq,d,NULL);
	return  (0);

   err_dev_create:
   		free(d);
   		return  (-1);
}


   


