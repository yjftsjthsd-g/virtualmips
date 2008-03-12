 /*
 * Copyright (C) yajin 2008 <yajinzhou@gmail.com >
 *     
 * This file is part of the virtualmips distribution. 
 * See LICENSE file for terms of the license. 
 *
 */

 /*JZ4740 Header file*/

#ifndef __JZ4740_H__
#define __JZ4740_H__



#include "types.h"

/*virtual address and physical address*/
typedef m_uint32_t m_va_t;
typedef m_uint32_t m_pa_t;
typedef m_uint32_t m_reg_t;
typedef m_int32_t m_ireg_t;
typedef m_uint32_t m_cp0_reg_t; 


#define  DATA_WIDTH 32 /*64*/
#define LL



/*Guest endian*/
#define GUEST_BYTE_ORDER  ARCH_LITTLE_ENDIAN
#ifndef GUEST_BYTE_ORDER
#error Please define guest architecture in utils.h!
#endif


/* Host to VM conversion functions */
#if HOST_BYTE_ORDER == GUEST_BYTE_ORDER
#define htovm16(x) (x)
#define htovm32(x) (x)
#define htovm64(x) (x)

#define vmtoh16(x) (x)
#define vmtoh32(x) (x)
#define vmtoh64(x) (x)
#elif HOST_BYTE_ORDER==ARCH_LITTLE_ENDIAN  //host:little guest:big
#define htovm16(x) (htons(x))
#define htovm32(x) (htonl(x))
#define htovm64(x) (swap64(x))

#define vmtoh16(x) (ntohs(x))
#define vmtoh32(x) (ntohl(x))
#define vmtoh64(x) (swap64(x))
#else   //host:big guest:little

#define htovm16(x) (ntohs(x))
#define htovm32(x) (ntohl(x))
#define htovm64(x) (swap64(x))

#define vmtoh16(x) (htons(x))
#define vmtoh32(x) (htonl(x))
#define vmtoh64(x) (swap64(x))
#endif


/*------------------------REG DEFINE---------------------------------*/
#define NAND_DATAPORT	0x18000000
#define NAND_ADDRPORT	0x18010000
#define NAND_COMMPORT	0x18008000






/*---------------------GPIO----------------------------------*/
#define JZ4740_GPIO_BASE       0x10010000
#define JZ4740_GPIO_SIZE       0x388
//n = 0,1,2,3
#define GPIO_PXPIN(n)	( (0x00 + (n)*0x100)) /* PIN Level Register */
#define GPIO_PXDAT(n)	((0x10 + (n)*0x100)) /* Port Data Register */
#define GPIO_PXDATS(n)	( (0x14 + (n)*0x100)) /* Port Data Set Register */
#define GPIO_PXDATC(n)	( (0x18 + (n)*0x100)) /* Port Data Clear Register */
#define GPIO_PXIM(n)	( (0x20 + (n)*0x100)) /* Interrupt Mask Register */
#define GPIO_PXIMS(n)	( (0x24 + (n)*0x100)) /* Interrupt Mask Set Reg */
#define GPIO_PXIMC(n)	( (0x28 + (n)*0x100)) /* Interrupt Mask Clear Reg */
#define GPIO_PXPE(n)	((0x30 + (n)*0x100)) /* Pull Enable Register */
#define GPIO_PXPES(n)	( (0x34 + (n)*0x100)) /* Pull Enable Set Reg. */
#define GPIO_PXPEC(n)	( (0x38 + (n)*0x100)) /* Pull Enable Clear Reg. */
#define GPIO_PXFUN(n)	( (0x40 + (n)*0x100)) /* Function Register */
#define GPIO_PXFUNS(n)	( (0x44 + (n)*0x100)) /* Function Set Register */
#define GPIO_PXFUNC(n)	( (0x48 + (n)*0x100)) /* Function Clear Register */
#define GPIO_PXSEL(n)	( (0x50 + (n)*0x100)) /* Select Register */
#define GPIO_PXSELS(n)	( (0x54 + (n)*0x100)) /* Select Set Register */
#define GPIO_PXSELC(n)	( (0x58 + (n)*0x100)) /* Select Clear Register */
#define GPIO_PXDIR(n)	( (0x60 + (n)*0x100)) /* Direction Register */
#define GPIO_PXDIRS(n)	( (0x64 + (n)*0x100)) /* Direction Set Register */
#define GPIO_PXDIRC(n)	( (0x68 + (n)*0x100)) /* Direction Clear Register */
#define GPIO_PXTRG(n)	( (0x70 + (n)*0x100)) /* Trigger Register */
#define GPIO_PXTRGS(n)	( (0x74 + (n)*0x100)) /* Trigger Set Register */
#define GPIO_PXTRGC(n)	( (0x78 + (n)*0x100)) /* Trigger Set Register */
#define GPIO_PXFLG(n)	( (0x80 + (n)*0x100)) /* Port Flag Register */
/* According to datasheet, it is 0x14. I think it shoud be 0x84*/
#define GPIO_PXFLGC(n)	( (0x84 + (n)*0x100)) /* Port Flag clear Register */

#define JZ4740_GPIO_INDEX_MAX  0xe2 /*0x388/4*/


/*---------------------UART----------------------------------*/
#define JZ4740_UART0_BASE       0x10030000
#define JZ4740_UART0_SIZE       0x2c
#define JZ4740_UART1_BASE       0x10031000
#define JZ4740_UART1_SIZE       0x2c
#define JZ4740_UART2_BASE       0x10032000
#define JZ4740_UART2_SIZE       0x2c
#define JZ4740_UART3_BASE       0x10033000
#define JZ4740_UART3_SIZE       0x2c

#define JZ4740_UART_BASE       JZ4740_UART0_BASE

#define UART_RBR         (0x00)  
#define UART_THR         (0x00)  
#define UART_DLLR        (0x00) 
#define UART_DLHR        (0x04) 
#define UART_IER         (0x04)  
#define UART_IIR         (0x08) 
#define UART_FCR         (0x08)  
#define UART_LCR         (0x0C)  
#define UART_MCR         (0x10)
#define UART_LSR         (0x14)  
#define UART_MSR         (0x18)  
#define UART_SPR         (0x1C) 
#define UART_ISR         (0x20) 
#define UART_UMR         (0x24)  
#define UART_UACR        (0x28)  

#define JZ4740_UART_INDEX_MAX							0xb //0x02c/4
#define JZ4740_UART_NUMBER                            2    //we emulates two uarts

#define UART_IER_RDRIE      0x01
#define UART_IER_TDRIE      0x02
#define UART_IER_RLSIE      0x04
#define UART_IER_MSIE      0x08
#define UART_IER_RTOIE      0x10

#define UART_FCR_FME   0x1
#define UART_FCR_RFRT   0x2
#define UART_FCR_TFRT   0x4
#define UART_FCR_DME   0x8
#define UART_FCR_UME   0x10
#define UART_FCR_RDTR   0xC0
#define UART_FCR_RDTR_SHIFT   0x6

#define UART_LSR_DRY   0x1
#define UART_LSR_OVER   0x2
#define UART_LSR_PARER   0x4
#define UART_LSR_FMER   0x8
#define UART_LSR_BI   0x10
#define UART_LSR_TDRQ   0x20
#define UART_LSR_TEMP   0x40
#define UART_LSR_FIFOE   0x80


/*-------------------PLL----------------------*/

#define JZ4740_CPM_BASE 0x10000000
#define JZ4740_CPM_SIZE 0X70

#define CPM_CPCCR       (0x00)
#define CPM_CPPCR       (0x10)
#define CPM_I2SCDR      (0x60)
#define CPM_LPCDR       (0x64)
#define CPM_MSCCDR      (0x68)
#define CPM_UHCCDR      (0x6C)

#define CPM_LCR         (0x04)
 #define CPM_CLKGR       (0x20)
#define CPM_SCR         (0x24)

#define CPM_HCR         (0x30)
#define CPM_HWFCR       (0x34)
 #define CPM_HRCR        (0x38)
 #define CPM_HWCR        (0x3c)
#define CPM_HWSR        (0x40)
#define CPM_HSPR        (0x44)

#define JZ4740_CPM_INDEX_MAX 0X1c /*0X70/4*/


/*-------------------EMC----------------------*/
#define JZ4740_EMC_BASE 0x13010000
#define JZ4740_EMC_SIZE 0xa024
#define EMC_BCR         ( 0x0)  /* BCR */
#define EMC_SMCR0       ( 0x10)  /* Static Memory Control Register 0 */
#define EMC_SMCR1       ( 0x14)  /* Static Memory Control Register 1 */
 #define EMC_SMCR2       ( 0x18)  /* Static Memory Control Register 2 */
 #define EMC_SMCR3       ( 0x1c)  /* Static Memory Control Register 3 */
#define EMC_SMCR4       ( 0x20)  /* Static Memory Control Register 4 */
#define EMC_SACR0       ( 0x30)  /* Static Memory Bank 0 Addr Config Reg */
#define EMC_SACR1       ( 0x34)  /* Static Memory Bank 1 Addr Config Reg */
#define EMC_SACR2       ( 0x38)  /* Static Memory Bank 2 Addr Config Reg */
#define EMC_SACR3       ( 0x3c)  /* Static Memory Bank 3 Addr Config Reg */
#define EMC_SACR4       ( 0x40)  /* Static Memory Bank 4 Addr Config Reg */

#define EMC_NFCSR       ( 0x050) /* NAND Flash Control/Status Register */
#define EMC_NFECR       ( 0x100) /* NAND Flash ECC Control Register */
#define EMC_NFECC       ( 0x104) /* NAND Flash ECC Data Register */
#define EMC_NFPAR0      ( 0x108) /* NAND Flash RS Parity 0 Register */
#define EMC_NFPAR1      ( 0x10c) /* NAND Flash RS Parity 1 Register */
#define EMC_NFPAR2      ( 0x110) /* NAND Flash RS Parity 2 Register */
#define EMC_NFINTS      ( 0x114) /* NAND Flash Interrupt Status Register */
#define EMC_NFINTE      ( 0x118) /* NAND Flash Interrupt Enable Register */
#define EMC_NFERR0      ( 0x11c) /* NAND Flash RS Error Report 0 Register */
#define EMC_NFERR1      ( 0x120) /* NAND Flash RS Error Report 1 Register */
#define EMC_NFERR2      ( 0x124) /* NAND Flash RS Error Report 2 Register */
#define EMC_NFERR3      ( 0x128) /* NAND Flash RS Error Report 3 Register */

#define EMC_DMCR        ( 0x80)  /* DRAM Control Register */
#define EMC_RTCSR       ( 0x84)  /* Refresh Time Control/Status Register */
#define EMC_RTCNT       ( 0x88)  /* Refresh Timer Counter */
#define EMC_RTCOR       ( 0x8c)  /* Refresh Time Constant Register */
#define EMC_DMAR0       ( 0x90)  /* SDRAM Bank 0 Addr Config Register */
#define EMC_SDMR0       ( 0xa000) /* Mode Register of SDRAM bank 0 */
/*has other register*/

#define JZ4740_EMC_INDEX_MAX 0x2809 /*0xa024/4*/



/*-----------------------RTC-------------------------------------*/

#define JZ4740_RTC_BASE 0x10003000
#define JZ4740_RTC_SIZE  0x38
#define RTC_RCR         ( 0x00) /* RTC Control Register */
#define RTC_RSR         ( 0x04) /* RTC Second Register */
#define RTC_RSAR        ( 0x08) /* RTC Second Alarm Register */
#define RTC_RGR         ( 0x0c) /* RTC Regulator Register */

#define RTC_HCR         ( 0x20) /* Hibernate Control Register */
#define RTC_HWFCR       ( 0x24) /* Hibernate Wakeup Filter Counter Reg */
#define RTC_HRCR        ( 0x28) /* Hibernate Reset Counter Register */
#define RTC_HWCR        ( 0x2c) /* Hibernate Wakeup Control Register */
#define RTC_HWRSR       ( 0x30) /* Hibernate Wakeup Status Register */
#define RTC_HSPR        ( 0x34) /* Hibernate Scratch Pattern Register */

#define JZ4740_RTC_INDEX_MAX 0xe /*0x38/4*/


/*----------------------WDT&TCU--------------------------------*/
#define JZ4740_WDT_TCU_BASE 0x10002000
#define JZ4740_WDT_TCU_SIZE  0xa0

#define WDT_TDR         ( 0x00)
#define WDT_TCER        ( 0x04)
#define WDT_TCNT        ( 0x08)
#define WDT_TCSR        ( 0x0C)

/*************************************************************************
 * TCU (Timer Counter Unit)
 *************************************************************************/
#define TCU_TSR		( 0x1C) /* Timer Stop Register */
#define TCU_TSSR	( 0x2C) /* Timer Stop Set Register */
#define TCU_TSCR	( 0x3C) /* Timer Stop Clear Register */
#define TCU_TER		( 0x10) /* Timer Counter Enable Register */
#define TCU_TESR	( 0x14) /* Timer Counter Enable Set Register */
#define TCU_TECR	( 0x18) /* Timer Counter Enable Clear Register */
#define TCU_TFR		( 0x20) /* Timer Flag Register */
#define TCU_TFSR	( 0x24) /* Timer Flag Set Register */
#define TCU_TFCR	( 0x28) /* Timer Flag Clear Register */
#define TCU_TMR		( 0x30) /* Timer Mask Register */
#define TCU_TMSR	( 0x34) /* Timer Mask Set Register */
#define TCU_TMCR	( 0x38) /* Timer Mask Clear Register */
#define TCU_TDFR0	( 0x40) /* Timer Data Full Register */
#define TCU_TDHR0	( 0x44) /* Timer Data Half Register */
#define TCU_TCNT0	( 0x48) /* Timer Counter Register */
#define TCU_TCSR0	( 0x4C) /* Timer Control Register */
#define TCU_TDFR1	( 0x50)
#define TCU_TDHR1	( 0x54)
#define TCU_TCNT1	( 0x58)
#define TCU_TCSR1	( 0x5C)
#define TCU_TDFR2	(0x60)
#define TCU_TDHR2	( 0x64)
#define TCU_TCNT2	( 0x68)
#define TCU_TCSR2	( 0x6C)
#define TCU_TDFR3	( 0x70)
#define TCU_TDHR3	( 0x74)
#define TCU_TCNT3	( 0x78)
#define TCU_TCSR3	( 0x7C)
#define TCU_TDFR4	( 0x80)
#define TCU_TDHR4	( 0x84)
#define TCU_TCNT4	( 0x88)
#define TCU_TCSR4	( 0x8C)
#define TCU_TDFR5	( 0x90)
#define TCU_TDHR5	( 0x94)
#define TCU_TCNT5	( 0x98)
#define TCU_TCSR5	( 0x9C)


#define JZ4740_WDT_INDEX_MAX 0x28  /*0xa0/4*/

#endif



