#ifndef __CONFIG_H__
#define __CONFIG_H__

#define COMMON_CONFIG_OPTION  \
           CFG_SIMPLE_INT("ram_size", &(vm->ram_size)),   \
			CFG_SIMPLE_INT("rom_size", &(vm->rom_size)),   \
			CFG_SIMPLE_STR("rom_file_name", &(vm->rom_filename)),  \
			CFG_SIMPLE_INT("rom_phy_address", &(vm->rom_address)),  \
			CFG_SIMPLE_INT("gdb_debug", &(vm->gdb_debug)),  \
			CFG_SIMPLE_INT("gdb_port", &(vm->gdb_port)),  \
 			CFG_SIMPLE_INT("flash_size", &(vm->flash_size)),   \
			CFG_SIMPLE_STR("flash_file_name", &(vm->flash_filename)),   \
			CFG_SIMPLE_INT("flash_phy_address", &(vm->flash_address)),   \
			CFG_SIMPLE_INT("boot_method", &(vm->boot_method)),   \
			CFG_SIMPLE_INT("boot_from", &(vm->boot_from)),   \
			CFG_SIMPLE_STR("kernel_file_name", &(vm->kernel_filename)),      \

#define  PRINT_COMMON_COFING_OPTION    \
    printf("Using configure file: %s\n",vm->configure_filename);  \
	printf("ram_size: %dM bytes \n",vm->ram_size);  \
	printf("boot_method: %s \n",boot_method_string[vm->boot_method-1]);  \
	if (vm->rom_size!=0)   \
	{   \
		printf("rom_size: %dM bytes \n",vm->rom_size);  \
		printf("rom_file_name: %s \n",vm->rom_filename);  \
		printf("rom_phy_address: 0x%x \n",vm->rom_address);   \
	}   \
	if (vm->flash_size!=0)   \
	{   \
		printf("flash_size: %dM bytes \n",vm->flash_size);   \
		printf("flash_file_name: %s \n",vm->flash_filename);   \
		printf("flash_phy_address: 0x%x \n",vm->flash_address);   \
	}   \
	if (vm->boot_method==BOOT_BINARY)   \
	{   \
		printf("boot_from: %s \n",boot_from_string[vm->boot_from-1]);   \
	}   \
	if (vm->boot_method==BOOT_ELF)  \
	{   \
		printf("kernel_file_name: %s \n",vm->kernel_filename);  \
	}   \
	if (vm->gdb_debug!=0)   \
	{   \
		printf("GDB debug enable\n");   \
		printf("GDB port: %d \n",vm->gdb_port);    \
	}   \

#define VALID_COMMON_CONFIG_OPTION    \
   ASSERT(vm->ram_size!=0,"ram_size can not be 0\n");   \
	if (vm->rom_size!=0)   \
	{   \
		ASSERT(vm->rom_filename!=NULL,"rom_file_name can not be NULL\n");   \
		ASSERT(vm->rom_address!=0,"rom_address can not be 0\n");   \
	}   \
	if (vm->flash_size!=0)   \
	{   \
		ASSERT(vm->flash_size==4,"flash_size should be 4.\n We only support 4MB flash emulation\n");   \
		/*ASSERT(vm->flash_filename!=NULL,"flash_file_name can not be NULL\n"); */  \
		/*flash_filename can be null. virtualmips will create it.*/   \
		ASSERT(vm->flash_address!=0,"flash_address can not be 0\n");   \
		if (vm->rom_size!=0)   \
		{   \
			ASSERT(vm->flash_address!=vm->rom_address,"flash_address can not be equal rom_address\n");   \
		}   \
	}   \
	ASSERT(vm->boot_method!=0,"boot_method can not be 0\n 1:binary  2:elf \n");   \
	if (vm->boot_method==BOOT_BINARY)     \
	{    \
		/*boot from binary image*/   \
		ASSERT(vm->boot_from!=0,"boot_from can not be 0\n 1:ROM  2:FLASH\n");   \
		if (vm->boot_from==BOOT_FROM_ROM)   \
		{   \
			ASSERT(vm->rom_size!=0,"rom_size can not be 0\n");   \
			ASSERT(vm->rom_filename!=NULL,"rom_file_name can not be NULL\n");   \
			ASSERT(vm->rom_address!=0,"rom_address can not be 0\n");   \
		}   \
		else if (vm->boot_from==BOOT_FROM_FLASH)   \
		{   \
			ASSERT(vm->flash_size!=0,"flash_size can not be 0\n");   \
			ASSERT(vm->flash_filename!=NULL,"flashm_filename can not be NULL\n");   \
			ASSERT(vm->flash_address!=0,"flash_address can not be 0\n");   \
		}   \
		else    \
			ASSERT(0,"error boot_from. valid value: 1:ROM  2:FLASH \n");   \
	}   \
	else if (vm->boot_method==BOOT_ELF)   \
	{   \
		ASSERT(vm->kernel_filename!=0,"kernel_file_name can not be NULL\n ");   \
	}   \
	else    \
		ASSERT(0,"error boot_method. valid value: 1:binary  2:elf \n");   \

#endif
