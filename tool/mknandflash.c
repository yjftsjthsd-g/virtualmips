

/*make u-boot into a nand flash.
for K9K8G nand flash device
*/
/*
we will write 1G bytes nand flash into 8192 files, 128k bytes data and 4k bytes spare data per file
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>

#define TOTAL_SIZE  0x42000000    /*1G data BYTES +32M bytes SPARE BYTES*/
#define TOTAL_PLANE 4
#define TOTAL_PAGES 0x80000
#define TOTAL_BLOCKS 0x2000
#define PAGES_PER_BLOCK  0x40


#define PAGE_SIZE   0x840    /*2k bytes date size+64 bytes spare size*/
#define SPARE_SIZE  0x40     /*64 bytes*/
#define BLOCK_SIZE  0x21000  /*132k bytes*/
#define PAGE_DATA_SIZE 0x800  /*2k bytes */
#define BLOCK_DATA_SIZE 0x20000  /*2k bytes */

#define PAGE_DATA_SIZE 0x800  /*2k bytes */
#define BLOCK_OFFSET(x)    (BLOCK_SIZE*x)
#define PAGE_OFFSET(x)     (PAGE_SIZE*x)


#define F16K 0X4000

char *u_boot_file_name=NULL;
void useage()
{
	printf("Useage:mknandflash -b u-boot image file name\n");
}

void parse_cmd_line(int argc,char *argv[])
{
	char *options_list = 
      "b:";
  int option;
	while((option = getopt(argc,argv,options_list)) != -1) 
  {
  	switch(option)
  	{
  		case 'b':
  			u_boot_file_name=strdup(optarg);
  			break;
  	}
  }
  if (NULL==u_boot_file_name)
	{
		printf("please set u_boot_file_name\n");
		useage();
		exit(-1);
	}
  printf("u_boot_file_name %s\n",u_boot_file_name);
}

struct spare_struct
{
	unsigned char spare_data[SPARE_SIZE];
};


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
	n_fp = fopen("flash/nandflash8g.0","w+");
	if (n_fp==NULL)
	{
		system("mkdir flash");
	}
	n_fp = fopen("flash/nandflash8g.0","w+");
	assert(n_fp!=NULL);
	fseek(n_fp,0,SEEK_SET);
	
	/*write first 16k to flash. page size 2k.*/
	for (i=0;i<8;i++)
	{
		fwrite(temp,1,PAGE_DATA_SIZE,n_fp);
		temp+=PAGE_DATA_SIZE;
		
		memset(&spare,0xff,sizeof(spare));
		/*write spare data*/
		if ((i==0)||(i==1))
		{
			/*1 and 2 page contains non-ff data*/
			spare.spare_data[0]=0x00;
			spare.spare_data[1]=0x00;
		}
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
	
	/*write other blocks*/
	for (j=1;j<TOTAL_BLOCKS;j++)
	{
		if (u_blocks==j)
			break;
		{
			/*create nand flash file*/
			sprintf(temp_name,"flash/nandflash8g.%d",j);
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
			if ((i==0)||(i==1))
			{
				/*1 and 2 page contains non-ff data*/
				spare.spare_data[0]=0x00;
				spare.spare_data[1]=0x00;
			}
			fwrite(&spare,1,sizeof(spare),n_fp);
		
		
		}
	}
	
	fclose(n_fp);
	free(temp);
	
	printf("done\n");
	
	
}
