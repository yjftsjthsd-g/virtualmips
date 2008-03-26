#ifndef __JZ4740_DEV_UART_H__
#define __JZ4740_DEV_UART_H__

#include "device.h"
#include "mips64_memory.h"
#include "jz4740.h"
#include "cpu.h"
#include "vp_timer.h"

struct jz4740_uart_data {
   struct vdevice *dev;

   u_int irq,duart_irq_seq;
   u_int output;

   vtty_t *vtty;
   vm_instance_t *vm;

   m_uint32_t  ier ;/*0x04*/
   m_uint32_t  iir ;/*0x08*/
   m_uint32_t  fcr ;/*0x08*/
   m_uint32_t  lcr ;/*0x0c*/
   m_uint32_t  mcr ;/*0x10*/
   m_uint32_t  lsr ;/*0x14*/
   m_uint32_t  msr ;/*0x18*/
   m_uint32_t  spr ;/*0x1c*/
   m_uint32_t  isr ;/*0x20*/
   m_uint32_t  umr ;/*0x24*/
   m_uint32_t  uacr ;/*0x28*/

   m_uint32_t jz4740_uart_size;

   vp_timer_t *uart_timer;

   

   
};

struct jz4740_uart_data * jz4740_get_uart0(cpu_mips_t *cpu);

#endif



