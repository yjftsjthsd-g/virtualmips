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

For rootfs of yaffs, it already contains spare bit of flash page. So just 
copy its content to flash. 
For other(u-boot and uImage), we just copy the content into flash and fill the spare by ourself.
*/




#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>

#include <confuse.h>

/*types from qemu*/
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

// Linux/Sparc64 defines uint64_t
#if !(defined (__sparc_v9__) && defined(__linux__))
/* XXX may be done for all 64 bits targets ? */
#if defined (__x86_64__) || defined(__ia64)
typedef unsigned long uint64_t;
#else
typedef unsigned long long uint64_t;
#endif
#endif

#ifndef __sun__
typedef signed char int8_t;
#endif
typedef signed short int16_t;
typedef signed int int32_t;
// Linux/Sparc64 defines int64_t
#if !(defined (__sparc_v9__) && defined(__linux__))
#if defined (__x86_64__) || defined(__ia64)
typedef signed long int64_t;
#else
typedef signed long long int64_t;
#endif
#endif



#define TOTAL_SIZE  0x42000000  /*1G data BYTES +32M bytes SPARE BYTES */
#define TOTAL_PLANE 4
#define TOTAL_PAGES 0x80000
#define TOTAL_BLOCKS 0x2000
#define PAGES_PER_BLOCK  0x40


#define PAGE_SIZE   0x840       /*2k bytes date size+64 bytes spare size */
#define SPARE_SIZE  0x40        /*64 bytes */
#define BLOCK_SIZE  0x21000     /*132k bytes */
#define PAGE_DATA_SIZE 0x800    /*2k bytes */
#define BLOCK_DATA_SIZE 0x20000 /*128k bytes */

#define BLOCK_OFFSET(x)    (BLOCK_SIZE*x)
#define PAGE_OFFSET(x)     (PAGE_SIZE*x)


#define F16K 0X4000

#define ASSERT(a,format,args...)  do{ if ((format!=NULL)&&(!(a)))   fprintf(stderr,format, ##args); assert((a));} while(0)

char *u_boot_file_name = NULL;
uint32_t u_boot_start_address = 0;
uint32_t u_boot_has_spare = 0;

char *kernel_file_name = NULL;
uint32_t kernel_start_address = 0;
uint32_t kernel_has_spare = 0;

char *rootfs_file_name = NULL;
uint32_t rootfs_start_address = 0;
uint32_t rootfs_has_spare = 0;

char *custom1_file_name = NULL;
uint32_t custom1_start_address = 0;
uint32_t custom1_has_spare = 0;

char *custom2_file_name = NULL;
uint32_t custom2_start_address = 0;
uint32_t custom2_has_spare = 0;

char *custom3_file_name = NULL;
uint32_t custom3_start_address = 0;
uint32_t custom3_has_spare = 0;


char *custom4_file_name = NULL;
uint32_t custom4_start_address = 0;
uint32_t custom4_has_spare = 0;

char *custom5_file_name = NULL;
uint32_t custom5_start_address = 0;
uint32_t custom5_has_spare = 0;



void parse_configure_file(int argc, char *argv[])
{
   cfg_opt_t opts[] = {
      CFG_SIMPLE_INT("uboot_startaddress", &u_boot_start_address),
      CFG_SIMPLE_STR("uboot_file", &(u_boot_file_name)),
      CFG_SIMPLE_INT("uboot_has_spare", &u_boot_has_spare),

      CFG_SIMPLE_INT("kernel_startaddress", &kernel_start_address),
      CFG_SIMPLE_STR("kernel_file", &(kernel_file_name)),
      CFG_SIMPLE_INT("kernel_has_spare", &kernel_has_spare),


      CFG_SIMPLE_INT("rootfs_startaddress", &rootfs_start_address),
      CFG_SIMPLE_STR("rootfs_file", &(rootfs_file_name)),
      CFG_SIMPLE_INT("rootfs_has_spare", &rootfs_has_spare),

      CFG_SIMPLE_INT("custom1_startaddress", &custom1_start_address),
      CFG_SIMPLE_STR("custom1_file", &(custom1_file_name)),
      CFG_SIMPLE_INT("custom1_has_spare", &custom1_has_spare),


      CFG_SIMPLE_INT("custom2_startaddress", &custom2_start_address),
      CFG_SIMPLE_STR("custom2_file", &(custom2_file_name)),
      CFG_SIMPLE_INT("custom2_has_spare", &custom2_has_spare),

      CFG_SIMPLE_INT("custom3_startaddress", &custom3_start_address),
      CFG_SIMPLE_STR("custom3_file", &(custom3_file_name)),
      CFG_SIMPLE_INT("custom3_has_spare", &custom3_has_spare),

      CFG_SIMPLE_INT("custom4_startaddress", &custom4_start_address),
      CFG_SIMPLE_STR("custom4_file", &(custom4_file_name)),
      CFG_SIMPLE_INT("custom4_has_spare", &custom4_has_spare),

      CFG_SIMPLE_INT("custom5_startaddress", &custom5_start_address),
      CFG_SIMPLE_STR("custom5_file", &(custom5_file_name)),
      CFG_SIMPLE_INT("custom5_has_spare", &custom5_has_spare),
      CFG_END()
   };
   cfg_t *cfg;
   cfg = cfg_init(opts, 0);
   cfg_parse(cfg, "nandflash.conf");
   cfg_free(cfg);

   ASSERT(u_boot_start_address == 0, "uboot_startaddress must be 0\n");
   ASSERT(u_boot_file_name != NULL, "please set u-boot file\n");

   if (custom1_start_address != 0)
      ASSERT(custom1_file_name != NULL, "please set custom1_file\n");
   if (custom1_file_name != NULL)
      ASSERT(custom1_start_address != 0, "please set custom1_startaddress\n");
   if (custom2_start_address != 0)
      ASSERT(custom2_file_name != NULL, "please set custom2_file\n");
   if (custom2_file_name != NULL)
      ASSERT(custom2_start_address != 0, "please set custom2_startaddress\n");
   if (custom3_start_address != 0)
      ASSERT(custom3_file_name != NULL, "please set custom3_file\n");
   if (custom3_file_name != NULL)
      ASSERT(custom3_start_address != 0, "please set custom3_startaddress\n");
   if (custom4_start_address != 0)
      ASSERT(custom4_file_name != NULL, "please set custom4_file\n");
   if (custom4_file_name != NULL)
      ASSERT(custom4_start_address != 0, "please set custom4_startaddress\n");
   if (custom5_start_address != 0)
      ASSERT(custom5_file_name != NULL, "please set custom5_file\n");
   if (custom5_file_name != NULL)
      ASSERT(custom5_start_address != 0, "please set custom5_startaddress\n");

   ASSERT((u_boot_start_address % BLOCK_DATA_SIZE) == 0, "uboot_startaddress must be block aligned\n");
   ASSERT((kernel_start_address % BLOCK_DATA_SIZE) == 0, "kernel_startaddress must be block aligned\n");
   ASSERT((rootfs_start_address % BLOCK_DATA_SIZE) == 0, "rootfs_startaddress must be block aligned\n");

   ASSERT((custom1_start_address % BLOCK_DATA_SIZE) == 0, "custom1_startaddress must be block aligned\n");
   ASSERT((custom2_start_address % BLOCK_DATA_SIZE) == 0, "custom2_startaddress must be block aligned\n");
   ASSERT((custom3_start_address % BLOCK_DATA_SIZE) == 0, "custom3_startaddress must be block aligned\n");
   ASSERT((custom4_start_address % BLOCK_DATA_SIZE) == 0, "custom4_startaddress must be block aligned\n");
   ASSERT((custom5_start_address % BLOCK_DATA_SIZE) == 0, "custom5_startaddress must be block aligned\n");


   printf("----configure options--------------\n");
   printf("uboot_startaddress 0x%x\n", u_boot_start_address);
   printf("uboot_file %s\n", u_boot_file_name);
   printf("uboot_has_spare %x\n", u_boot_has_spare);

   printf("kernel_startaddress 0x%x\n", kernel_start_address);
   printf("kernel_file %s\n", kernel_file_name);
   printf("kernel_has_spare %x\n", kernel_has_spare);

   printf("rootfs_startaddress 0x%x\n", rootfs_start_address);
   printf("rootfs_file %s\n", rootfs_file_name);
   printf("rootfs_has_spare %x\n", rootfs_has_spare);

   printf("custom1_startaddress 0x%x\n", custom1_start_address);
   printf("custom1_file %s\n", custom1_file_name);
   printf("custom1_has_spare %x\n", custom1_has_spare);


   printf("custom2_startaddress 0x%x\n", custom2_start_address);
   printf("custom2_file %s\n", custom2_file_name);
   printf("custom2_has_spare %x\n", custom2_has_spare);

   printf("custom3_startaddress 0x%x\n", custom3_start_address);
   printf("custom3_file %s\n", custom3_file_name);
   printf("custom3_has_spare %x\n", custom3_has_spare);

   printf("custom4_startaddress 0x%x\n", custom4_start_address);
   printf("custom4_file %s\n", custom4_file_name);
   printf("custom4_has_spare %x\n", custom4_has_spare);

   printf("custom5_startaddress 0x%x\n", custom4_start_address);
   printf("custom5_file %s\n", custom4_file_name);
   printf("custom5_has_spare %x\n", custom5_has_spare);



}




/*given the start address and file size, cal the block start no and block end no*/
void cal_block_no(uint32_t start_address, uint32_t file_size, uint32_t * block_start_no, uint32_t * block_end_no)
{
   *block_start_no = start_address / BLOCK_DATA_SIZE;

   *block_end_no = *block_start_no + (file_size / BLOCK_DATA_SIZE);
   if ((file_size % BLOCK_DATA_SIZE) == 0)
      (*block_end_no)--;

}

/*yaffs2 file has sparece already*/
int write_nand_flash_file_with_sparce(char *file_name, unsigned int start_address)
{
   FILE *n_fp;                  /*fp for nand flash */
   FILE *fp;
   uint32_t file_size;
   uint32_t block_start_no, block_end_no;
   char block_file_name[64];
   unsigned char block[BLOCK_SIZE];
   int i;
   int leave = 0;

   fp = fopen(file_name, "r");
   if (fp == NULL)
   {
      printf("can not open file %s\n", file_name);
      return (-1);
   }
   fseek(fp, 0, SEEK_END);
   file_size = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   cal_block_no(start_address, file_size, &block_start_no, &block_end_no);
   for (i = block_start_no; i <= block_end_no; i++)
   {
      snprintf(block_file_name, sizeof(block_file_name), "nandflash1GB/nandflash1GB.%d", i);


      n_fp = fopen(block_file_name, "w+");
      assert(n_fp != NULL);
      fseek(n_fp, 0, SEEK_SET);
      memset(block, 0xff, BLOCK_SIZE);
      /*copy block from file */
      leave = file_size - (i - block_start_no) * BLOCK_SIZE;
      if (leave >= 0)
         fread(block, 1, BLOCK_SIZE, fp);
      else
         fread(block, 1, leave + BLOCK_SIZE, fp);
      fwrite(block, 1, BLOCK_SIZE, n_fp);

      fclose(n_fp);


   }

   fclose(fp);
   return (0);

}

int write_nand_flash_file(char *file_name, unsigned int start_address)
{
   FILE *n_fp;                  /*fp for nand flash */
   FILE *fp;
   uint32_t file_size;
   int i, j;
   unsigned char page[PAGE_SIZE];
   unsigned char spare_page[SPARE_SIZE];
   char block_file_name[64];

   uint32_t block_start_no, block_end_no;
   int leave = 0;

   fp = fopen(file_name, "r");
   if (fp == NULL)
   {
      printf("can not open file %s\n", file_name);
      return (-1);
   }
   fseek(fp, 0, SEEK_END);
   file_size = ftell(fp);
   assert(file_size >= F16K);   /*u-boot image must >F16K */
   fseek(fp, 0, SEEK_SET);
   cal_block_no(start_address, file_size, &block_start_no, &block_end_no);
   for (i = block_start_no; i <= block_end_no; i++)
   {
      snprintf(block_file_name, sizeof(block_file_name), "nandflash1GB/nandflash1GB.%d", i);

      n_fp = fopen(block_file_name, "w+");
      assert(n_fp != NULL);
      fseek(n_fp, 0, SEEK_SET);

      for (j = 0; j < PAGES_PER_BLOCK; j++)
      {
         memset(page, 0xff, PAGE_SIZE);
         leave = file_size - (i - block_start_no) * BLOCK_DATA_SIZE - j * PAGE_DATA_SIZE;
         if (leave >= 0)
         {
            fread(page, 1, PAGE_DATA_SIZE, fp);
         }
         else                   /*last page */
         {
            fread(page, 1, leave + PAGE_DATA_SIZE, fp);
         }


         if (!strcmp(file_name, u_boot_file_name))
         {
            /*first 16k */
            /*spare 2-4 bytes: 00 means valid page. JZ4740 boot rom spec */
            if ((i == 0) && (j < 8))
            {
               page[PAGE_DATA_SIZE + 2] = 0x0;
               page[PAGE_DATA_SIZE + 3] = 0x0;
               page[PAGE_DATA_SIZE + 4] = 0x0;

            }
         }


         fwrite(page, 1, sizeof(page), n_fp);


      }
      fclose(n_fp);


   }
   fclose(fp);
   return (0);

}

int main(int argc, char *argv[])
{


   parse_configure_file(argc, argv);

   system("rm -rf nandflash1GB");
   system("mkdir nandflash1GB");


   /*write u-boot */
   if (u_boot_file_name != NULL)
   {
      if (u_boot_has_spare)
      {
         if (write_nand_flash_file_with_sparce(u_boot_file_name, u_boot_start_address) == -1)
            exit(-1);
      }
      else
      {
         if (write_nand_flash_file(u_boot_file_name, u_boot_start_address) == -1)
            exit(-1);
      }
   }


   if (kernel_file_name != NULL)
   {
      if (kernel_has_spare)
      {
         if (write_nand_flash_file_with_sparce(kernel_file_name, kernel_start_address) == -1)
            exit(-1);
      }
      else
      {
         if (write_nand_flash_file(kernel_file_name, kernel_start_address) == -1)
            exit(-1);
      }
   }

   if (rootfs_file_name != NULL)
   {
      if (rootfs_has_spare)
      {
         if (write_nand_flash_file_with_sparce(rootfs_file_name, rootfs_start_address) == -1)
            exit(-1);
      }
      else
      {
         if (write_nand_flash_file(rootfs_file_name, rootfs_start_address) == -1)
            exit(-1);
      }
   }

   if (custom1_file_name != NULL)
   {
      if (custom1_has_spare)
      {
         if (write_nand_flash_file_with_sparce(custom1_file_name, custom1_start_address) == -1)
            exit(-1);
      }
      else
      {
         if (write_nand_flash_file(custom1_file_name, custom1_start_address) == -1)
            exit(-1);
      }
   }

   if (custom2_file_name != NULL)
   {
      if (custom2_has_spare)
      {
         if (write_nand_flash_file_with_sparce(custom2_file_name, custom2_start_address) == -1)
            exit(-1);
      }
      else
      {
         if (write_nand_flash_file(custom2_file_name, custom2_start_address) == -1)
            exit(-1);
      }
   }

   if (custom3_file_name != NULL)
   {
      if (custom3_has_spare)
      {
         if (write_nand_flash_file_with_sparce(custom3_file_name, custom3_start_address) == -1)
            exit(-1);
      }
      else
      {
         if (write_nand_flash_file(custom3_file_name, custom3_start_address) == -1)
            exit(-1);
      }
   }

   if (custom4_file_name != NULL)
   {
      if (custom4_has_spare)
      {
         if (write_nand_flash_file_with_sparce(custom4_file_name, custom4_start_address) == -1)
            exit(-1);
      }
      else
      {
         if (write_nand_flash_file(custom4_file_name, custom4_start_address) == -1)
            exit(-1);
      }
   }

   if (custom5_file_name != NULL)
   {
      if (custom5_has_spare)
      {
         if (write_nand_flash_file_with_sparce(custom5_file_name, custom5_start_address) == -1)
            exit(-1);
      }
      else
      {
         if (write_nand_flash_file(custom5_file_name, custom5_start_address) == -1)
            exit(-1);
      }
   }






   printf("done\n");





}
