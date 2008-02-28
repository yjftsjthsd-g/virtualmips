 /*
 * Copyright (C) yajin 2008 <yajinzhou@gmail.com >
 *     
 * This file is part of the virtualmips distribution. 
 * See LICENSE file for terms of the license. 
 *
 */

#define _GNU_SOURCE
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

#include "vm.h"
#include "utils.h"
#include "device.h"
#include "mips64_memory.h"


/* ROM private data */
struct rom_data {
	struct vdevice *dev;
	m_uint8_t *rom_ptr;
	m_uint32_t rom_size;
};
typedef struct rom_data  rom_data_t;

void *dev_rom_access(cpu_mips_t *cpu,struct vdevice *dev,
		m_uint32_t offset,u_int op_size,u_int op_type,
		m_reg_t *data,m_uint8_t *has_set_value)
{
	struct rom_data *d = dev->priv_data;

	if (op_type == MTS_WRITE) {
		cpu_log(cpu,"ROM","write attempt at address 0x%x (data=0x%x)\n",
				dev->phys_addr+offset,*data);
		return NULL;
	}

	if (offset >= d->rom_size) {
		*data = 0;
		return NULL;
	}

	return((void *)(d->rom_ptr + offset));
}

static int dev_rom_load(char * rom_file_name,m_uint32_t rom_len,unsigned char ** rom_data_hp,u_int create)
{
	int fd;
	struct stat sb;
	unsigned char *temp;

	fd=open(rom_file_name,O_RDWR );
	if ((fd<0)&&(create==1))
	{
		fprintf(stderr,"Can not open rom file. name %s\n", rom_file_name);
		fprintf(stderr,"creating rom file. name %s\n", rom_file_name);
		fd=open(rom_file_name, O_RDWR|O_CREAT, S_IRWXU);
		if (fd<0)
		{
			fprintf(stderr,"Can not create rom file. name %s\n", rom_file_name);
			return (-1);
		}
		temp=malloc(rom_len);
		assert(temp!=NULL);
		memset(temp,0xff,rom_len);
		lseek(fd,0,SEEK_SET);
		write(fd,(void*)temp,rom_len);
		free(temp);
		fprintf(stderr,"create rom file success. name %s\n", rom_file_name);
		lseek(fd,0,SEEK_SET);
	}
	else if (fd<0)
	{
		fprintf(stderr,"%s does not exist and not allowed to create.\n", rom_file_name);
		return (-1);
	}
	assert(fd>=0);
	fstat(fd,&sb);
	if (rom_len<sb.st_size)
	{
		fprintf(stderr,"Too large rom file. Rom len:%d M, Rom file name %s,"\
				"rom file legth: %d bytes.\n",rom_len,rom_file_name,(int)sb.st_size);
		return (-1);
	}
	*rom_data_hp=mmap(NULL,sb.st_size,PROT_WRITE|PROT_READ ,MAP_SHARED,fd,0);
	if ( *rom_data_hp==MAP_FAILED)
	{
		fprintf(stderr,"errno %d\n",errno );
		fprintf(stderr,"failed\n");
		return (-1);
	}  
	return 0;
}


/* Initialize a ROM zone */
int dev_rom_init(vm_instance_t *vm,char *name)
{
	rom_data_t *d;
	unsigned char *rom_data_hp;
	assert(vm->rom_filename!=NULL);
	assert(vm->rom_size>0);

	/*load rom data*/
	if ((dev_rom_load(vm->rom_filename,vm->rom_size*1048576,&rom_data_hp,0))==-1)
		return (-1);

	/* allocate the private data structure */
	if (!(d = malloc(sizeof(*d)))) {
		fprintf(stderr,"FLASH: unable to create device.\n");
		return (-1);
	}

	memset(d,0,sizeof(*d));
	d->rom_ptr  = rom_data_hp;
	d->rom_size = vm->rom_size*1048576;
	//d->state=ROM_INIT_STATE;

	if (!(d->dev = dev_create(name)))
		goto err_dev_create;
	d->dev->priv_data = d;
	d->dev->phys_addr = vm->rom_address;
	d->dev->phys_len  = vm->rom_size*1048576;
	d->dev->handler   = dev_rom_access;
	d->dev->flags     = VDEVICE_FLAG_NO_MTS_MMAP;

	/* Map this device to the VM */
	vm_bind_device(vm,d->dev);
	return (0);
	err_dev_create:
	free(d);
	return (-1);


}
