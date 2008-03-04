
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


m_uint8_t  id_info[5]={0xec,0xd3,0x51,0x95,0x58};


static int create_nand_flash_1g_file(m_uint32_t block_no,nand_flash_1g_data_t *d)
{
  	char file_path[64];
  	char page[NAND_FLASH_1G_PAGE_SIZE];
  	int i;
  	/*create nand flash file when writing*/
   snprintf(file_path,sizeof(file_path),"%s/%d",NAND_FLASH_1G_FILE_DIR,block_no);
   fd=open(file_path,O_RDWR|O_CREAT);
   assert(fd>=0);

  

   for (i=0;i<NAND_FLASH_1G_PAGES_PER_BLOCK;i++)
    {
       memset(page,0xff,NAND_FLASH_1G_PAGE_SIZE);
      if ((i==0)||(i==1))
        {
          /*spare page*/
          page[NAND_FLASH_1G_PAGE_DATA_SIZE]=0x00;
          page[NAND_FLASH_1G_PAGE_DATA_SIZE+1]=0x00;
        }
      write(fd,page,NAND_FLASH_1G_PAGE_SIZE);
    }

   return fd;

}
static m_iptr_t * dev_nand_flash_1g_write_data(nand_flash_1g_data_t *d)
{

  
  int fd;
  m_uint32_t block_no = (d->row_addr)>>6;
  m_uint32_t page_no = (d->row_addr)&0x3f;
  assert(block_no<NAND_FLASH_1G_TOTAL_BLOCKS);
  assert(d->col_addr<NAND_FLASH_1G_PAGE_SIZE);

  
  if ((*(d->flash_map+block_no))==NULL)
    {
      fd=create_nand_flash_1g_file(block_no,d);
      *(d->flash_map+block_no)=(m_iptr_t)memzone_map_file(fd,NAND_FLASH_1G_BLOCK_SIZE);
   }
  memcpy(d->flash_map+block_no+page_no*NAND_FLASH_1G_PAGE_SIZE+d->col_addr,
                d->write_buffer+d->col_addr,
                NAND_FLASH_1G_PAGE_SIZE-d->col_addr);
  

}



static m_iptr_t * dev_nand_flash_1g_page_ipr(nand_flash_1g_data_t *d)
{
  /*get pointer accoring to address*/
  
  m_uint32_t block_no = (d->row_addr)>>6;
  m_uint32_t page_no = (d->row_addr)&0x3f;
  assert(block_no<NAND_FLASH_1G_TOTAL_BLOCKS);
  return (d->flash_map+block_no+page_no*NAND_FLASH_1G_PAGE_SIZE);
}


void *dev_nand_flash_1g_access(cpu_mips_t *cpu,struct vdevice *dev,
		m_uint32_t offset,u_int op_size,u_int op_type,
		m_uint32_t *data,m_uint8_t *has_set_value)
{
    nand_flash_1g_data_t *d = dev->priv_data;
    
   /*COMMAND PORT*/
   if (offset==0x8000)
    {
      switch (d->state)
        {
          case STATE_INIT:
             if (((*data)&0xff)==0x00)
                d->state=STATE_READ_START;
             else if (((*data)&0xff)==0x80)
                d->state=STATE_WRITE_START;
             else if  (((*data)&0xff)==0x05)
              {
                assert(d->has_issue_30h==1);
                d->state=STATE_RANDOM_READ_START;
                d->has_issue_30h=0;
              }
             else
              assert(0);
           break;

           case STATE_READ_START:
            if  (((*data)&0xff)==0x30)
              {
                d->has_issue_30h=1;
                d->state=STATE_INIT;
                d->data_port_ipr=dev_nand_flash_1g_page_ipr(d);
                d->read_offset=d->col_addr;
              }
           else if  (((*data)&0xff)==0x35)
              {
                d->state=STATE_READ_PAGE_FOR_COPY_WRITE;
                /*copy 1 page data to internal page for write*/
                memcpy(d->internal_page,dev_nand_flash_1g_page_ipr(d),NAND_FLASH_1G_PAGE_SIZE);
              }
            else
              assert(0);
            break;

          case STATE_RANDOM_READ_START:
            if  (((*data)&0xff)==0xe0)
              {
                d->state=STATE_INIT;
                d->data_port_ipr=dev_nand_flash_1g_page_ipr(d);
                d->read_offset=d->col_addr;
              }
            else if  (((*data)&0xff)==0x05)
              {
                d->state=STATE_RANDOM_READ_START;
              }
            else
              assert(0);
            break;

         case STATE_WRITE_START:
            if  (((*data)&0xff)==0x10)
              {
                d->state=STATE_INIT;
                dev_nand_flash_1g_write_data(d);
              }
           else if  (((*data)&0xff)==0x85)
              {
                d->write_offset=d->col_addr;
                d->state=STATE_RANDOM_WRITE_START;
              }
            else
              assert(0);
            break;

          case STATE_RANDOM_WRITE_START:
               if  (((*data)&0xff)==0x10)
              {
                d->state=STATE_INIT;
                dev_nand_flash_1g_write_data(d);
              }
              else if  (((*data)&0xff)==0x85)
                {
                  d->write_offset=d->col_addr;
                  d->state=STATE_RANDOM_WRITE_START;
                }
              else
              assert(0);
            break;


         case STATE_READ_PAGE_FOR_COPY_WRITE:
           if  (((*data)&0xff)==0x85)
            {
              d->state=STATE_COPY_START;
            }
           else 
            assert(0);
           break;

           case STATE_COPY_START:
            if  (((*data)&0xff)==0x10)
              {
                d->state=STATE_INIT;
                /*copy internal page to write buffer*/
                memcpy(d->write_buffer,d->internal_page,NAND_FLASH_1G_PAGE_SIZE);
                dev_nand_flash_1g_write_data(d);
              }
              else if  (((*data)&0xff)==0x85)
                {
                  d->write_offset=d->col_addr;
                  d->state=STATE_RANDOM_WRITE_START;
                }
              else
              assert(0);
            break;

         case 0x90:
          d->data_port_ipr=&id_info[0];
          d->read_offset=0;
          break;

         default:
            assert(0);
      }
      return NULL;
     *has_set_value=TRUE;
      
    }
   else if (offset==0)
    {
      if (op_type==MTS_READ)
        {
          /*data port*/
          return (void*)(d->data_port_ipr+d->read_offset);
          d->read_offset++;
        }
      else if (op_type==MTS_WRITE)
        {
           return (void*)(d->write_buffer+d->write_offset);
          d->write_offset++;
        }
      assert(0);
      
    }


   
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

int dev_nand_flash_1g_init(vm_instance_t *vm,char *name,m_pa_t phys_addr,m_uint32_t phys_len,nand_flash_1g_data_t **nand_flash)
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
	d->state=STATE_INIT;
	memset(d->internal_page,0xff,NAND_FLASH_1G_PAGE_SIZE);
	memset(d->write_buffer,0xff,NAND_FLASH_1G_PAGE_SIZE);

	if (!(d->dev = dev_create(name)))
		goto err_dev_create;
	d->dev->priv_data = d;
	d->dev->handler   = dev_nand_flash_1g_access;
	/*NAND COMMPORT AND DATA PORT ADDRESS*/
	d->dev->phys_addr =phys_addr;
	d->dev->phys_len  = phys_len;
	d->dev->flags     = VDEVICE_FLAG_NO_MTS_MMAP;
	/* Map this device to the VM */
	vm_bind_device(vm,d->dev);
	


	*nand_flash=d;
	
	return(0);
	err_dev_create:
	free(d);
	return (-1);
}

