#ifndef __DEV_NAND_FLASH_1G_H__
#define __DEV_NAND_FLASH_1G_H__

#include "utils.h"



/* nand flash private data */
struct nand_flash_1g_data {
	struct vdevice *dev;
	m_iptr_t *flash_map;
};
typedef struct nand_flash_1g_data  nand_flash_1g_data_t;

int dev_nand_flash_1g_init(vm_instance_t *vm,char *name,nand_flash_1g_data_t *d);

#endif
