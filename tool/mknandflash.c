 /*
 * Copyright (C) yajin 2008 <yajinzhou@gmail.com >
 *     
 * This file is part of the virtualmips distribution. 
 * See LICENSE file for terms of the license. 
 *
 */
 

/*make u-boot into a nand flash.
for K9K8G nand flash device.

Useage:
  mknandflash 

please set nandflash.conf before!!


*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>

#include "confuse.h"

#define TOTAL_SIZE  0x42000000    /*1G data BYTES +32M bytes SPARE BYTES*/
#define TOTAL_PLANE 4
#define TOTAL_PAGES 0x80000
#define TOTAL_BLOCKS 0x2000
#define PAGES_PER_BLOCK  0x40


#define PAGE_SIZE   0x840    /*2k bytes date size+64 bytes spare size*/
#define SPARE_SIZE  0x40     /*64 bytes*/
#define BLOCK_SIZE  0x21000  /*132k bytes*/
#define PAGE_DATA_SIZE 0x800  /*2k bytes */
#define BLOCK_DATA_SIZE 0x20000  /*128k bytes */

#define BLOCK_OFFSET(x)    (BLOCK_SIZE*x)
#define PAGE_OFFSET(x)     (PAGE_SIZE*x)


#define F16K 0X4000

#define ASSERT(a,format,args...)  do{ if ((format!=NULL)&&(!(a)))   fprintf(stderr,format, ##args); assert((a));} while(0) 

char *u_boot_file_name=NULL;
unsigned int  u_boot_start_address=0;
char *kernel_file_name=NULL;
unsigned int  kernel_start_address=0;
char *rootfs_file_name=NULL;
unsigned int  rootfs_start_address=0;
char *custom1_file_name=NULL;
unsigned int  custom1_start_address=0;
char *custom2_file_name=NULL;
unsigned int  custom2_start_address=0;
char *custom3_file_name=NULL;
unsigned int  custom3_start_address=0;
char *custom4_file_name=NULL;
unsigned int  custom4_start_address=0;
char *custom5_file_name=NULL;
unsigned int  custom5_start_address=0;




void parse_configure_file(int argc,char *argv[])
{
  cfg_opt_t opts[] = {
			CFG_SIMPLE_INT("uboot_startaddress", &u_boot_start_address),   
			CFG_SIMPLE_STR("uboot_file", &(u_boot_file_name)),   
			CFG_SIMPLE_INT("kernel_startaddress", &kernel_start_address),   
			CFG_SIMPLE_STR("kernel_file", &(kernel_file_name)),   
			CFG_SIMPLE_INT("rootfs_startaddress", &rootfs_start_address),   
			CFG_SIMPLE_STR("rootfs_file", &(rootfs_file_name)),   
			CFG_SIMPLE_INT("custom1_startaddress", &custom1_start_address),   
			CFG_SIMPLE_STR("custom1_file", &(custom1_file_name)),   
			CFG_SIMPLE_INT("custom2_startaddress", &custom2_start_address),   
			CFG_SIMPLE_STR("custom2_file", &(custom2_file_name)),   
			CFG_SIMPLE_INT("custom3_startaddress", &custom3_start_address),   
			CFG_SIMPLE_STR("custom3_file", &(custom3_file_name)),   
			CFG_SIMPLE_INT("custom4_startaddress", &custom4_start_address),   
			CFG_SIMPLE_STR("custom4_file", &(custom4_file_name)),   
			CFG_SIMPLE_INT("custom5_startaddress", &custom5_start_address),   
			CFG_SIMPLE_STR("custom5_file", &(custom5_file_name)),   
			CFG_END()
	};
	cfg_t *cfg;
	cfg = cfg_init(opts, 0);
	cfg_parse(cfg, "nandflash.conf");
	cfg_free(cfg);
	
    ASSERT(u_boot_start_address==0,"uboot_startaddress must be 0\n");
	ASSERT(u_boot_file_name!=NULL,"please set u-boot file\n");
	//ASSERT(kernel_file_name!=NULL,"please set kernel file\n");
   //ASSERT(kernel_start_address!=0,"please set kernel start address\n");
   if (custom1_start_address!=0)
     ASSERT(custom1_file_name!=NULL,"please set custom1_file\n");
   if (custom1_file_name!=NULL)
     ASSERT(custom1_start_address!=0,"please set custom1_startaddress\n");
   if (custom2_start_address!=0)
     ASSERT(custom2_file_name!=NULL,"please set custom2_file\n");
   if (custom2_file_name!=NULL)
     ASSERT(custom2_start_address!=0,"please set custom2_startaddress\n");
   if (custom3_start_address!=0)
     ASSERT(custom3_file_name!=NULL,"please set custom3_file\n");
   if (custom3_file_name!=NULL)
     ASSERT(custom3_start_address!=0,"please set custom3_startaddress\n");
   if (custom4_start_address!=0)
     ASSERT(custom4_file_name!=NULL,"please set custom4_file\n");
   if (custom4_file_name!=NULL)
     ASSERT(custom4_start_address!=0,"please set custom4_startaddress\n");
   if (custom5_start_address!=0)
     ASSERT(custom5_file_name!=NULL,"please set custom5_file\n");
   if (custom5_file_name!=NULL)
     ASSERT(custom5_start_address!=0,"please set custom5_startaddress\n");
   
   ASSERT((u_boot_start_address%BLOCK_DATA_SIZE)==0,"uboot_startaddress must be block aligned\n");
   ASSERT((kernel_start_address%BLOCK_DATA_SIZE)==0,"kernel_startaddress must be block aligned\n");
   ASSERT((rootfs_start_address%BLOCK_DATA_SIZE)==0,"rootfs_startaddress must be block aligned\n");

   ASSERT((custom1_start_address%BLOCK_DATA_SIZE)==0,"custom1_startaddress must be block aligned\n");
   ASSERT((custom2_start_address%BLOCK_DATA_SIZE)==0,"custom2_startaddress must be block aligned\n");
   ASSERT((custom3_start_address%BLOCK_DATA_SIZE)==0,"custom3_startaddress must be block aligned\n");
   ASSERT((custom4_start_address%BLOCK_DATA_SIZE)==0,"custom4_startaddress must be block aligned\n");
   ASSERT((custom5_start_address%BLOCK_DATA_SIZE)==0,"custom5_startaddress must be block aligned\n");
   

	printf("----configure options--------------\n");
	printf("uboot_startaddress 0x%x\n",u_boot_start_address);
	printf("uboot_file %s\n",u_boot_file_name);
	printf("kernel_startaddress 0x%x\n",kernel_start_address);
	printf("kernel_file %s\n",kernel_file_name);
	printf("rootfs_startaddress 0x%x\n",rootfs_start_address);
	printf("rootfs_file %s\n",rootfs_file_name);
	printf("custom1_startaddress 0x%x\n",custom1_start_address);
	printf("custom1_file %s\n",custom1_file_name);
	printf("custom2_startaddress 0x%x\n",custom2_start_address);
	printf("custom2_file %s\n",custom2_file_name);
	printf("custom3_startaddress 0x%x\n",custom3_start_address);
	printf("custom3_file %s\n",custom3_file_name);
	printf("custom4_startaddress 0x%x\n",custom4_start_address);
	printf("custom4_file %s\n",custom4_file_name);
	printf("custom5_startaddress 0x%x\n",custom4_start_address);
	printf("custom5_file %s\n",custom4_file_name);
	


	
}




/*given the start address and file size, cal the block start no and block end no*/
void cal_block_no(unsigned int start_address,unsigned int file_size,unsigned int *block_start_no,unsigned int*block_end_no)
{
  *block_start_no=start_address/BLOCK_DATA_SIZE;
  
  *block_end_no=*block_start_no+(file_size/BLOCK_DATA_SIZE);
    if ((file_size%BLOCK_DATA_SIZE)==0)
      (*block_end_no)--;
  
}


int write_nand_flash_file(char *file_name,unsigned int start_address)
{
  FILE *n_fp;/*fp for nand flash*/
  FILE *fp;
  unsigned int file_size;
  int i,j;
  unsigned char page[PAGE_SIZE];
  unsigned char spare_page[SPARE_SIZE];
  char block_file_name[64];

  unsigned int block_start_no,block_end_no;
  int leave=0;

  fp=fopen(file_name,"r");
  if (fp==NULL)
  {
    printf("can not open file %s\n",file_name);
    return(-1);
  }
  fseek(fp,0, SEEK_END);
  file_size = ftell(fp);
  assert(file_size>=F16K);  /*u-boot image must >F16K*/
  fseek(fp,0,SEEK_SET);
  cal_block_no(start_address, file_size, &block_start_no, &block_end_no);
  
  for (i=block_start_no;i<=block_end_no;i++)
    {
      snprintf(block_file_name,sizeof(block_file_name),"nandflash1GB/nandflash1GB.%d",i);
      n_fp = fopen(block_file_name,"w+");
      if (n_fp==NULL)
        {
          system("mkdir nandflash1GB");
        }
      n_fp = fopen(block_file_name,"w+");
      assert(n_fp!=NULL);
      fseek(n_fp,0,SEEK_SET);
      
      for (j=0;j<PAGES_PER_BLOCK;j++)
        {
           memset(page,0xff,PAGE_SIZE);
           leave=file_size-(i-block_start_no)*BLOCK_DATA_SIZE-j*PAGE_DATA_SIZE;
           if (leave>=0)
            {
              fread(page,1,PAGE_DATA_SIZE,fp);
              //printf("j %x leave %x\n",j,leave);
            }
           else  /*last page*/
            {
              fread(page,1,leave+PAGE_DATA_SIZE,fp);
              //printf("here j %x leave %x leave+PAGE_DATA_SIZE %x\n",j,leave,leave+PAGE_DATA_SIZE);
            }


            if (!strcmp(file_name,u_boot_file_name))
           {
             /*first 16k*/
              /*spare 2-4 bytes: 00 means valid page. JZ4740 boot rom spec*/
              if ((i==0)&&(j<8))
                {
                  page[PAGE_DATA_SIZE+2]=0x0;
                  page[PAGE_DATA_SIZE+3]=0x0;
                  page[PAGE_DATA_SIZE+4]=0x0;
              
                }
             }

            
             fwrite(page,1,sizeof(page),n_fp);
             

        }
      fclose(n_fp);

     
    }
  fclose(fp);
  return (0);

}

int main(int argc,char*argv[])
{
  
  
  parse_configure_file(argc,argv);
  

  /*write u-boot*/
  if (write_nand_flash_file(u_boot_file_name,u_boot_start_address)==-1)
    exit(-1);
  if (kernel_file_name!=NULL)
    if (write_nand_flash_file(kernel_file_name,kernel_start_address)==-1)
     exit(-1);
if (rootfs_file_name!=NULL)
   if (write_nand_flash_file(rootfs_file_name,rootfs_start_address)==-1)
    exit(-1);
if (custom1_file_name!=NULL)
   if (write_nand_flash_file(custom1_file_name,custom1_start_address)==-1)
    exit(-1);
if (custom2_file_name!=NULL)
    if (write_nand_flash_file(custom2_file_name,custom2_start_address)==-1)
    exit(-1);
if (custom3_file_name!=NULL)
     if (write_nand_flash_file(custom3_file_name,custom3_start_address)==-1)
    exit(-1);
if (custom4_file_name!=NULL)
      if (write_nand_flash_file(custom4_file_name,custom4_start_address)==-1)
    exit(-1);
if (custom5_file_name!=NULL)
       if (write_nand_flash_file(custom5_file_name,custom5_start_address)==-1)
    exit(-1);

  


   printf("done\n");
  

        
  
  
}

#if 0

int main(int argc,char*argv[])
{
        FILE *u_fp;/*fp for u-boot image*/
        FILE *n_fp;/*fp for nand flash*/
        
        unsigned int u_size,u_blocks;
        unsigned char *temp=NULL;
        struct spare_struct spare;
        unsigned int i,j;

        
        unsigned char perpage[PAGE_DATA_SIZE];
        unsigned char temp_name[64];
        
        parse_cmd_line(argc,argv);
        
        u_fp = fopen(u_boot_file_name,"r");
        if (u_fp==NULL)
        {
                printf("can not open u-boot image file %s\n",u_boot_file_name);
                exit(-1);
        }
        fseek(u_fp,0, SEEK_END);
        u_size = ftell(u_fp);
        assert(u_size>=F16K);  /*u-boot image must >F16K*/
        
        u_blocks=(u_size/BLOCK_DATA_SIZE);
        if ((u_size%BLOCK_DATA_SIZE)!=0)
                u_blocks++;
        
        temp=malloc(u_blocks*BLOCK_DATA_SIZE);
        assert(temp!=NULL);
        memset(temp,0xff,u_blocks*BLOCK_DATA_SIZE);
        fseek(u_fp,0,SEEK_SET);
        fread(temp,1,u_size,u_fp);
        fclose(u_fp);
        
        
        /*read nand flash file */
        n_fp = fopen("nandflash1GB/nandflash1GB.0","w+");
        if (n_fp==NULL)
        {
                system("mkdir nandflash1GB");
        }
        n_fp = fopen("nandflash1GB/nandflash1GB.0","w+");
        assert(n_fp!=NULL);
        fseek(n_fp,0,SEEK_SET);
        
        /*write first 16k to flash. page size 2k.*/
        for (i=0;i<8;i++)
        {
                fwrite(temp,1,PAGE_DATA_SIZE,n_fp);
                temp+=PAGE_DATA_SIZE;
                
                memset(&spare,0xff,sizeof(spare));
                /*2-4 bytes: 00 means valid page*/
                spare.spare_data[2]=0x00;
                spare.spare_data[3]=0x00;
                spare.spare_data[4]=0x00;
                
                /*we do not check ECC value during booting.so just leave the other bits zero*/
                /*TODO:Reed-Solomon ECC*/
                fwrite(&spare,1,sizeof(spare),n_fp);
        }
        
        /*write other pages of 1 block*/
        for (i=8;i<PAGES_PER_BLOCK;i++)
        {
                fwrite(temp,1,PAGE_DATA_SIZE,n_fp);
                temp+=PAGE_DATA_SIZE;
                
                memset(&spare,0xff,sizeof(spare));
                fwrite(&spare,1,sizeof(spare),n_fp);
        }
        
        /*write other u-boot blocks*/
        for (j=1;j<u_blocks;j++)
        {
		         
               // if (u_blocks==j)
               //         break;
                {
                        /*create nand flash file*/
                        sprintf(temp_name,"nandflash1GB/nandflash1GB.%d",j);
                        fclose(n_fp);
                        n_fp = fopen(temp_name,"w+");
                        assert(n_fp!=NULL);
                        fseek(n_fp,0,SEEK_SET);
                }
                for (i=0;i<PAGES_PER_BLOCK;i++)
                {
                        //printf("j %d i %d u_pages %d\n",j,i,u_pages);
                        fwrite(temp,1,PAGE_DATA_SIZE,n_fp);
                        temp+=PAGE_DATA_SIZE;
        
                        memset(&spare,0xff,sizeof(spare));
                        /*write spare data*/
                        fwrite(&spare,1,sizeof(spare),n_fp);
                
                
                }
        }
        
        fclose(n_fp);

        
        printf("done\n");
        
        
}
#endif

