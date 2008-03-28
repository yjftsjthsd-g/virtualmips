#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include "crc.h"
#include "utils.h"
#include "cpu.h"
#include "vm.h"
#include "mips64_memory.h"
#include "device.h"
#include "net.h"
#include "net_io.h"
#include "dev_cs8900.h"

/*
CS8900A is in bigendian.
host is little endian(x86)
*/



#define CS8900_INTERNAL_RAM_SIZE   0x1000  /*4K*/
#define CS8900A_PRODUCT_ID  0x630e          /*little endian*/

#define PP_RT_DATA0              0x00
#define PP_RT_DATA1              0x02
#define PP_TX_CMD              0X04
#define PP_TX_LEN              0X06
#define PP_ISQ              0X08
 #define PP_ADDRESS              0x0a    /* PacketPage Pointer Port (Section 4.10.10) */
 #define PP_DATA0                 0x0c    /* PacketPage Data Port (Section 4.10.10) */
  #define PP_DATA1               0X0e  

 #define PP_ProductID            0x0000  /* Section 4.3.1   Product Identification Code */
 #define PP_ISAIOB 					0x0020        /*  IO base address */
 #define PP_IntNum                       0x0022  /* Section 3.2.3   Interrupt Number */
 #define PP_ISASOF						 0x0026        /*  ISA DMA offset */
 #define PP_DmaFrameCnt 				0x0028   /*  ISA DMA Frame count */
 #define PP_DmaByteCnt 					0x002A    /*  ISA DMA Byte count */
 #define PP_MemBase                      0x002c  /* Section 4.9.2   Memory Base Address Register */
 #define PP_EEPROMCommand        0x0040  /* Section 4.3.11  EEPROM Command */
 #define PP_EEPROMData           0x0042  /* Section 4.3.12  EEPROM Data */





 
 #define PP_RxCFG                        0x0102  /* Section 4.4.6   Receiver Configuration */
 #define PP_RxCTL                        0x0104  /* Section 4.4.8   Receiver Control */
 #define PP_TxCFG                        0x0106  /* Section 4.4.9   Transmit Configuration */
 #define PP_BufCFG                       0x010a  /* Section 4.4.12  Buffer Configuration */
 #define PP_LineCTL                      0x0112  /* Section 4.4.16  Line Control */
 #define PP_SelfCTL                      0x0114  /* Section 4.4.18  Self Control */
 #define PP_BusCTL                       0x0116  /* Section 4.4.20  Bus Control */
 #define PP_TestCTL                      0x0118  /* Section 4.4.22  Test Control */
 #define PP_AutoNegCTL 				0x011C    /*  Auto Negotiation Ctrl */
 #define PP_ISQ                           0x0120  /* Section 4.4.5   Interrupt Status Queue */
 #define PP_RxEvent 							0x0124       /*  Rx Event Register */
 #define PP_TxEvent                      0x0128  /* Section 4.4.10  Transmitter Event */
 #define PP_BufEvent                     0x012c  /* Section 4.4.13  Buffer Event */
 #define PP_RxMISS                       0x0130  /* Section 4.4.14  Receiver Miss Counter */
 #define PP_TxCOL                        0x0132  /* Section 4.4.15  Transmit Collision Counter */
 #define PP_LineST							 0x0134        /*  Line State Register */
 #define PP_SelfST                       0x0136  /* Section 4.4.19  Self Status */
 #define PP_BusST                        0x0138  /* Section 4.4.21  Bus Status */
 #define PP_TDR 								0x013C           /*  Time Domain Reflectometry */
  #define PP_AutoNegST 				0x013E     /*  Auto Neg Status */
 #define PP_TxCMD                        0x0144  /* Section 4.4.11  Transmit Command */
 #define PP_TxLength                     0x0146  /* Section 4.5.2   Transmit Length */
 #define PP_LAF								 0x0150           /*  Hash Table */
 #define PP_IA                           	 0x0158  /* Section 4.6.2   Individual Address (IEEE Address) */

 #define PP_RxStatus                     0x0400  /* Section 4.7.1   Receive Status */
 #define PP_RxLength                     0x0402  /* Section 4.7.1   Receive Length (in bytes) */
 #define PP_RxFrame                      0x0404  /* Section 4.7.2   Receive Frame Location */
 #define PP_TxFrame                      0x0a00  /* Section 4.7.2   Transmit Frame Location */


/*CS8900 net card*/

/* DEC21140 Data */
struct cs8900_data {
   char *name;
   m_uint32_t cs8900_size;

   /* Device information */
   struct vdevice *dev;
  
   /* Virtual machine */
   vm_instance_t *vm;

   /* NetIO descriptor */
   netio_desc_t *nio;

   /* TX ring scanner task id */
   //ptask_id_t tx_tid;

	/*internal RAM 4K bytes*/
   m_uint32_t internal_ram[CS8900_INTERNAL_RAM_SIZE/4];
   m_uint32_t irq_no;

   //m_uint16_t packet_page_index;
   m_uint16_t rx_read_index;
   m_uint16_t tx_send_index;
   
};



static int dev_cs8900_handle_rxring(netio_desc_t *nio,
                                      u_char *pkt,ssize_t pkt_len,
                                      struct cs8900_data *d)
{

}

#define VALIDE_CS8900_OPERATION 0
static void *dev_cs8900_access(cpu_mips_t *cpu,struct vdevice *dev,
		m_uint32_t offset,u_int op_size,u_int op_type,
		m_uint32_t *data,m_uint8_t *has_set_value)
{
	struct cs8900_data *d = dev->priv_data;
	void * ret;
	
	if (offset >= d->cs8900_size) {
      *data = 0;
      return NULL;
   }

#if  VALIDE_CS8900_OPERATION
	if (op_type==MTS_WRITE)
	{
		ASSERT(offset!=PP_ISQ,"Write to read only register in CS8900. offset %x\n",offset);
	}
	else if (op_type==MTS_READ)
	{
		ASSERT(offset!=PP_TX_CMD,"Read write only register in CS8900. offset %x\n",offset);
		ASSERT(offset!=PP_TX_LEN,"Read write only register in CS8900. offset %x\n",offset);
	}
#endif	

switch (offset)
{
	case PP_RT_DATA0:
	case PP_RT_DATA1:
		if (offset==PP_RT_DATA0)
			ASSERT (op_size==MTS_HALF_WORD,"op_size must be 2. op_size %x\n",op_size);
		else if (offset==PP_RT_DATA1)
			ASSERT (op_size==MTS_WORD,"op_size must be 4. op_size %x\n",op_size);
		if (op_type==MTS_READ)
		{
			ASSERT(d->rx_read_index<*(d->internal_ram+PP_RxLength),"read out of data rx_read_index %x data len %x \n",d->rx_read_index,*(d->internal_ram+PP_RxLength));
			ret= (void *)(d->internal_ram+PP_RxFrame+d->rx_read_index);
			d->rx_read_index +=op_size;
			/****if read all data,set d->rx_read_index=0*/
			if (d->rx_read_index==*(d->internal_ram+PP_RxLength))
				d->rx_read_index=0;
			
			return ret;
		}
		else if(op_type==MTS_WRITE)
		{
			ret= (void *)(d->internal_ram+PP_TxFrame+d->tx_send_index);
			d->tx_send_index +=op_size;
			/*if write all data into tx buffer, set d->rx_read_index=0. generate interrupt if required*/
			
		}
}

	
}

static void dev_cs8900_init_defaultvalue(struct cs8900_data *d)
{
	m_uint8_t * ram_base;

	ram_base = (m_uint8_t *)(&(d->internal_ram[0]));

	*(m_uint32_t*)(ram_base+PP_ProductID)= CS8900A_PRODUCT_ID;
	*(m_uint16_t*)(ram_base+PP_ISAIOB)= 0x300;
	*(m_uint16_t*)(ram_base+PP_IntNum)= 0x4;
	*(m_uint16_t*)(ram_base+PP_IntNum)= 0x4;

	*(m_uint16_t*)(ram_base+PP_RxCFG)= 0x3;
	*(m_uint16_t*)(ram_base+PP_RxEvent)= 0x4;

	*(m_uint16_t*)(ram_base+PP_RxCTL)= 0x5;
	*(m_uint16_t*)(ram_base+PP_TxCFG)= 0x7;
	*(m_uint16_t*)(ram_base+PP_TxEvent)= 0x8;
	*(m_uint16_t*)(ram_base+0x108)= 0x9;

	*(m_uint16_t*)(ram_base+PP_BufCFG)= 0xb;
	*(m_uint16_t*)(ram_base+PP_BufEvent)= 0xc;

	*(m_uint16_t*)(ram_base+PP_RxMISS)= 0x10;

	*(m_uint16_t*)(ram_base+PP_TxCOL)= 0x12;
	*(m_uint16_t*)(ram_base+PP_LineCTL)= 0x13;
	*(m_uint16_t*)(ram_base+PP_LineST)= 0x14;
	*(m_uint16_t*)(ram_base+PP_SelfCTL)= 0x15;

	*(m_uint16_t*)(ram_base+PP_SelfST)= 0x16;
	*(m_uint16_t*)(ram_base+PP_BusCTL)= 0x17;

	*(m_uint16_t*)(ram_base+PP_BusST)= 0x18;
	*(m_uint16_t*)(ram_base+PP_TestCTL)= 0x19;

	*(m_uint16_t*)(ram_base+PP_TDR)= 0x1c;

	*(m_uint16_t*)(ram_base+PP_TxCMD)= 0x9;
   /*00:62:9c:61:cf:16*/
	*(ram_base+PP_IA)= 0x00;
   *(ram_base+PP_IA+1)= 0x62;
   *(ram_base+PP_IA+2)= 0x9c;
   *(ram_base+PP_IA+3)= 0x61;
   *(ram_base+PP_IA+4)= 0xcf;
   *(ram_base+PP_IA+5)= 0x16;
	
	
	
}
static void dev_cs8900_reset(cpu_mips_t *cpu,struct vdevice *dev)
{
	struct cs8900_data *d=dev->priv_data;
	memset(d->internal_ram,0,sizeof(d->internal_ram));
	dev_cs8900_init_defaultvalue(d);
}
struct cs8900_data *dev_cs8900_init(vm_instance_t *vm,char *name,m_pa_t phys_addr,m_uint32_t phys_len,
                                        int irq)
{
   struct cs8900_data *d;
   struct vdevice *dev;

   /* Allocate the private data structure for DEC21140 */
   if (!(d = malloc(sizeof(*d)))) {
      fprintf(stderr,"%s (cs8900_data): out of memory\n",name);
      return NULL;
   }
   memset(d,0,sizeof(*d));

   /* Create the device itself */
   if (!(dev = dev_create(name))) {
      fprintf(stderr,"%s (DEC21140): unable to create device.\n",name);
      goto err_dev;
   }

   d->dev->priv_data = d;
   d->dev->phys_addr = phys_addr;
   d->dev->phys_len = phys_len;
   d->cs8900_size=phys_len;
   d->irq_no=irq;
   
   d->dev->handler   = dev_cs8900_access;
   d->dev->reset_handler   = dev_cs8900_reset;
   
   d->dev->flags     = VDEVICE_FLAG_NO_MTS_MMAP;
   
	vm_bind_device(vm,d->dev);
   return(d);

 err_dev:
   free(d);
   return NULL;
}


/* Bind a NIO to cs8900 device */
int dev_cs8900_set_nio(struct cs8900_data *d,netio_desc_t *nio)
{   
   /* check that a NIO is not already bound */
   if (d->nio != NULL)
      return(-1);

   d->nio = nio;
   //d->tx_tid = ptask_add((ptask_callback)dev_dec21140_handle_txring,d,NULL);
   netio_rxl_add(nio,(netio_rx_handler_t)dev_cs8900_handle_rxring,d,NULL);
   return(0);
}

/* Unbind a NIO from a cs8900 device */
void dev_cs8900_unset_nio(struct cs8900_data *d)
{
   if (d->nio != NULL) {
      //ptask_remove(d->tx_tid);
      netio_rxl_remove(d->nio);
      d->nio = NULL;
   }
}











