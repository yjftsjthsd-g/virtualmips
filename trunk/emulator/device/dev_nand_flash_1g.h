#ifndef __DEV_NAND_FLASH_1G_H__
#define __DEV_NAND_FLASH_1G_H__

#include "utils.h"


#define NAND_FLASH_1G_TOTAL_SIZE  0x42000000    /*1G data BYTES +32M bytes SPARE BYTES*/
#define NAND_FLASH_1G_TOTAL_PLANE 4
#define NAND_FLASH_1G_TOTAL_PAGES 0x80000
#define NAND_FLASH_1G_TOTAL_BLOCKS 0x2000
#define NAND_FLASH_1G_PAGES_PER_BLOCK  0x40


#define NAND_FLASH_1G_PAGE_SIZE   0x840    /*2k bytes date size+64 bytes spare size*/
#define NAND_FLASH_1G_SPARE_SIZE  0x40     /*64 bytes*/
#define NAND_FLASH_1G_BLOCK_SIZE  0x21000  /*132k bytes*/
#define NAND_FLASH_1G_PAGE_DATA_SIZE 0x800  /*2k bytes */
#define NAND_FLASH_1G_BLOCK_DATA_SIZE 0x20000  /*128k bytes */

#define NAND_FLASH_1G_BLOCK_OFFSET(x)    (NAND_FLASH_1G_BLOCK_SIZE*x)
#define NAND_FLASH_1G_PAGE_OFFSET(x)     (NAND_FLASH_1G_PAGE_SIZE*x)

#define NAND_FLASH_1G_FILE_PREFIX   	    "nandflash1GB"
#define NAND_FLASH_1G_FILE_DIR                  "nandflash1GB"

/* nand flash private data */
struct nand_flash_1g_data {
	struct vdevice *dev;
	m_iptr_t *flash_map;
};
typedef struct nand_flash_1g_data  nand_flash_1g_data_t;

int dev_nand_flash_1g_init(vm_instance_t *vm,char *name,nand_flash_1g_data_t **nand_flash);

#endif
