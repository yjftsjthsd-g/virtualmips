#ifndef __PAVO_H__
#define __PAVO_H__

#include "jz4740.h"

#define PAVO_DEFAULT_CONFIG_FILE     "pavo.conf"
#define PAVO_DEFAULT_RAM_SIZE           16
#define PAVO_DEFAULT_BOOT_METHOD     BOOT_BINARY
#define PAVO_DEFAULT_KERNEL_FILENAME     "vmlinux"
#define PAVO_ADDR_BUS_MASK   0xffffffff   /*32bit phy address*/

#define PAVO_CONFIG0  0x80000082
#define PAVO_CONFIG1 0x3E613080  /*CACHE (128SET*32 BYTES*2 WAY)= 8K*/

#define SOC_CONFIG0 PAVO_CONFIG0
#define SOC_CONFIG1 PAVO_CONFIG1

#define PAVO_ROM_PC  0xbfc00000UL
#define PAVO_PRID    0x0001800b  
#define PAVO_DEFAULT_TLB_ENTRYNO   16  /*16 pairs*/


struct pavo_system {
   /* Associated VM instance */
   vm_instance_t *vm;
};


typedef struct pavo_system pavo_t;


#define VM_PAVO(vm) ((pavo_t *)vm->hw_data)


vm_instance_t *create_instance(char *conf);
int init_instance(vm_instance_t *vm);
void  virtual_timer(cpu_mips_t *cpu);

#endif
