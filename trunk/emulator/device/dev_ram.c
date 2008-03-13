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
#include<sys/types.h>
#include<sys/stat.h>
#include<string.h>

#include "vm.h"
#include "mips64_memory.h"
#include "device.h"

/* RAM private data */
struct ram_data {
	struct vdevice *dev;
};


/* Initialize a RAM zone */
int dev_ram_init(vm_instance_t *vm,char *name,m_pa_t paddr,m_uint32_t len)
{
	struct ram_data *d;

	/* allocate the private data structure */
	if (!(d = malloc(sizeof(*d)))) {
		fprintf(stderr,"RAM: unable to create device.\n");
		return(-1);
	}

	memset(d,0,sizeof(*d));
 
	if (!(d->dev =dev_create_ram(vm,name,paddr,len))) {
		fprintf(stderr,"RAM: unable to create device.\n");
		goto err_dev_create;
	}
   d->dev->priv_data = d;   
	return(0);

	err_dev_create:
	free(d);
	return(-1);
}


