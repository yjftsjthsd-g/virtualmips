
/*
1G bytes nand flash emulation. Samsung K9F8G08 1GB
1G bytes nand flash are stored in file nandflash8g.0-nandflash8g.8191(8192 blocks).
The flash file only be created when writing to block(copy on write).
Please use tool/mknandflash to create init nand file of u-boot image.
*/


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include<fcntl.h>

#include "device.h"
#include "mips64_memory.h"#i


void *dev_nand_flash_1g_access(cpu_mips_t *cpu,struct vdevice *dev,
		m_uint32_t offset,u_int op_size,u_int op_type,
		m_uint32_t *data,m_uint8_t *has_set_value)
{
  
}




int dev_nand_flash_1g_init(vm_instance_t *vm,char *name,nand_flash_1g_data_t *d)
{
	//nand_flash_1g_data_t *d;

	/* allocate the private data structure */
	if (!(d = malloc(sizeof(*d)))) {
		fprintf(stderr,"NAND FLASH: unable to create device.\n");
		return (-1);
	}
    /*load all flash data to d->flash_map*/

    
	memset(d,0,sizeof(*d));
	
	if (!(d->dev = dev_create(name)))
		goto err_dev_create;
	d->dev->priv_data = d;
	/*we do not look up nand flash accoring to its address.
	In order to compliant with vdevice model ,set its phys_addr to a big value.*/
	d->dev->phys_addr = 0xfffffffe;
	d->dev->phys_len = 0x1;
	d->dev->handler   = dev_nand_flash_1g_access;

	/* Map this device to the VM */
	vm_bind_device(vm,d->dev);
	return(0);
	err_dev_create:
	free(d);
	return (-1);
}

