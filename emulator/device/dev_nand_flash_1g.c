
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
#include <dirent.h>




#include "device.h"
#include "mips64_memory.h"


void *dev_nand_flash_1g_access(cpu_mips_t *cpu,struct vdevice *dev,
		m_uint32_t offset,u_int op_size,u_int op_type,
		m_uint32_t *data,m_uint8_t *has_set_value)
{
  
}


static int load_nand_flash_file(nand_flash_1g_data_t *d)
{
	int i,j=0;
	struct dirent* ent = NULL;
	DIR *p_dir;
	char file_path[64];
	char *file_name;
	char block_number[16];
	int fd;

	
	d->flash_map=malloc(NAND_FLASH_1G_TOTAL_BLOCKS*sizeof(*d->flash_map));
	if (d->flash_map==NULL)
	{
		fprintf(stderr,"NAND FLASH: Can not alloc space for flash_map.\n");
		return (-1);
	}
	memset(d->flash_map,0x0,NAND_FLASH_1G_TOTAL_BLOCKS*sizeof(*d->flash_map));
	p_dir=opendir(NAND_FLASH_1G_FILE_DIR);
	if (NULL==p_dir)
	{
		fprintf(stderr,"NAND FLASH: Can not open nand flash file directory \"%s\".\n",NAND_FLASH_1G_FILE_DIR);
		goto err_flash_map_create;
	}
	while (NULL!= (ent=readdir(p_dir)))
	{
		//we only take care file
		if (ent->d_type==DT_DIR)
			continue; 
		snprintf(file_path,sizeof(file_path),"%s/%s",NAND_FLASH_1G_FILE_DIR,ent->d_name);
		if (get_file_size(file_path)!=NAND_FLASH_1G_BLOCK_SIZE)
			continue;
		file_name=strdup(ent->d_name);
		for (i=strlen(file_name)-1;i>=0;i--)
		{
			if (file_name[i]=='.')
				break;
		}
		if (i==-1)
		{
			//not a valid flash file
			continue;
		}
		file_name[i]='\0';
		if (strcmp(file_name,NAND_FLASH_1G_FILE_PREFIX)!=0)
		{
			continue;
		}
		file_name=strdup(ent->d_name);
		//get the block number
		strncpy(block_number,file_name+i+1,strlen(file_name)-i-1);
		i = atoi(block_number);
		fd=open(file_path,O_RDWR);
		if (fd<0)
			goto err_open_flash_file;
		*(d->flash_map+i)=(m_iptr_t)memzone_map_file(fd,NAND_FLASH_1G_BLOCK_SIZE);
		if (*(d->flash_map+i)==0)
			goto err_map_flash_file;
		close(fd);
		free(file_name);
		//printf("%x\n",*(d->flash_map+i));
		j++;
		
	}
	printf("\nloaded %d nand flash file from directory \"%s\". \n",j,NAND_FLASH_1G_FILE_DIR);
	return (0);
err_map_flash_file:
	close(fd);
err_open_flash_file:
	free(file_name);
err_flash_map_create:
	free(d->flash_map);
	return (-1);
	
}

int dev_nand_flash_1g_init(vm_instance_t *vm,char *name,nand_flash_1g_data_t **nand_flash)
{
	
	nand_flash_1g_data_t *d;

	/* allocate the private data structure */
	if (!(d = malloc(sizeof(*d)))) {
		fprintf(stderr,"NAND FLASH: unable to create device.\n");
		return (-1);
	}
    /*load all flash data to d->flash_map*/
	if (load_nand_flash_file(d)==-1)
		return (-1);
    
	memset(d,0,sizeof(*d));
	
	if (!(d->dev = dev_create(name)))
		goto err_dev_create;
	d->dev->priv_data = d;
	d->dev->handler   = dev_nand_flash_1g_access;

	*nand_flash=d;
	
	return(0);
	err_dev_create:
	free(d);
	return (-1);
}

