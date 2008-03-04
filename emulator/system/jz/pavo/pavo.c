
#define _GNU_SOURCE
#include<string.h>
#include <assert.h>
#include<stdlib.h>

#include "confuse.h"
#include "utils.h"
#include "mips64.h"
#include "vm.h"
#include "cpu.h"
#include "mips64_exec.h"
#include "debug.h"

#include "pavo.h"
#include "device.h"



/* Initialize default parameters for a adm5120 */
static void pavo_init_defaults(pavo_t *pavo)
{
	vm_instance_t *vm = pavo->vm;

	if (vm->configure_filename==NULL)
		vm->configure_filename=strdup(PAVO_DEFAULT_CONFIG_FILE);
	vm->ram_size        = PAVO_DEFAULT_RAM_SIZE;
	vm->boot_method = PAVO_DEFAULT_BOOT_METHOD;
	vm->kernel_filename=strdup(PAVO_DEFAULT_KERNEL_FILENAME);
}



/* Initialize the PAVO Platform (MIPS) */
static int pavo_init_platform(pavo_t *pavo)
{
	struct vm_instance *vm = pavo->vm;
	cpu_mips_t *cpu0; 
	void *(*cpu_run_fn)(void *);



	vm_init_vtty(vm);


	/* Create a CPU group */
	vm->cpu_group = cpu_group_create("System CPU");

	/* Initialize the virtual MIPS processor */
	if (!(cpu0 = cpu_create(vm,CPU_TYPE_MIPS32,0))) {
		vm_error(vm,"unable to create CPU0!\n");
		return(-1);
	}
	/* Add this CPU to the system CPU group */
	cpu_group_add(vm->cpu_group,cpu0);
	vm->boot_cpu = cpu0;


	cpu_run_fn = (void *)mips64_exec_run_cpu;
	/* create the CPU thread execution */
	if (pthread_create(&cpu0->cpu_thread,NULL,cpu_run_fn,cpu0) != 0) {
		fprintf(stderr,"cpu_create: unable to create thread for CPU%u\n",0);
		free(cpu0);
		return (-1);
	}
	cpu0->addr_bus_mask = PAVO_ADDR_BUS_MASK;

	/* Initialize RAM */
	vm_ram_init(vm,0x00000000ULL);

	/*create 1GB nand flash*/
	if ((vm->flash_size==0x400)&&(vm->flash_type=FLASH_TYPE_NAND_FLASH))
	  if (dev_nand_flash_1g_init(vm,"NAND FLASH 1G",0xb8000000,0x10004,&(pavo->nand_flash))==-1)
	    return (-1);

	return(0);
}


static void pavo_reg_default_value(pavo_t *pavo)
{
	
}

static int pavo_boot(pavo_t *pavo)
{   
	vm_instance_t *vm = pavo->vm;
	cpu_mips_t *cpu;
	m_va_t kernel_entry_point;

	if (!vm->boot_cpu)
		return(-1);

	vm_suspend(vm);

	/* Check that CPU activity is really suspended */
	if (cpu_group_sync_state(vm->cpu_group) == -1) {
		vm_error(vm,"unable to sync with system CPUs.\n");
		return(-1);
	}

	/* Reset the boot CPU */
	cpu = (vm->boot_cpu);
	mips64_reset(cpu);

	/*set PC and PRID*/
	cpu->cp0.reg[MIPS_CP0_PRID] = PAVO_PRID;
	cpu->cp0.tlb_entries =  PAVO_DEFAULT_TLB_ENTRYNO;
	cpu->pc =  PAVO_ROM_PC;
   /*If we boot from elf kernel image, load the image and set pc to elf entry*/
	if (vm->boot_method==BOOT_ELF)
	{
		if (mips64_load_elf_image(cpu,vm->kernel_filename,
				&kernel_entry_point)==-1)
			return (-1);
		pavo_reg_default_value(pavo);
		cpu->pc=kernel_entry_point;
	}
	else if (vm->boot_method==BOOT_BINARY)
	  {
	  
	  }

	/* Launch the simulation */
	printf("\nPAVO '%s': starting simulation (CPU0 PC=0x%"LL"x), "
			"JIT %sabled.\n",
			vm->name,cpu->pc,vm->jit_use ? "en":"dis");

	vm->status = VM_STATUS_RUNNING;
	cpu_start(vm->boot_cpu);
	return(0);

}






void pavo_clear_irq(vm_instance_t *vm,u_int irq)
{


}


/*We must map adm irq to mips irq before setting irq*/
void pavo_set_irq(vm_instance_t *vm,u_int irq)
{


}

COMMON_CONFIG_INFO_ARRAY;
static void printf_configure(pavo_t *pavo)
{

	vm_instance_t *vm=pavo->vm;
	PRINT_COMMON_COFING_OPTION;

	/*print other configure information here*/

}
static void pavo_parse_configure(pavo_t *pavo)
{
	vm_instance_t *vm=pavo->vm;
	cfg_opt_t opts[] = {
			COMMON_CONFIG_OPTION
		  /*add other configure information here*/

			CFG_END()
	};
	cfg_t *cfg;

	cfg = cfg_init(opts, 0);
	cfg_parse(cfg, vm->configure_filename);
	cfg_free(cfg);

	VALID_COMMON_CONFIG_OPTION;

	/*add other configure information validation here*/
	if (vm->boot_method==BOOT_BINARY)
      {
        ASSERT(vm->boot_from==2,"boot_from must be 2(NAND Flash)\n pavo only can boot from NAND Flash.\n");   \
      }


    /*Print the configure information*/
	printf_configure(pavo);

}

/* Create a router instance */
vm_instance_t *create_instance(char *configure_filename)
{
	pavo_t *pavo;
	char *name;
	if (!(pavo = malloc(sizeof(*pavo)))) {
		fprintf(stderr,"ADM5120': Unable to create new instance!\n");
		return NULL;
	}

	memset(pavo,0,sizeof(*pavo));
	name=strdup("pavo");

	if (!(pavo->vm = vm_create(name,VM_TYPE_PAVO))) {
		fprintf(stderr,"PAVO : unable to create VM instance!\n");
		goto err_vm;
	}

	if (configure_filename!=NULL)
		pavo->vm->configure_filename=strdup(configure_filename);
	pavo_init_defaults(pavo);
	pavo_parse_configure(pavo);
	/*init gdb debug*/
	vm_debug_init(pavo->vm);


	pavo->vm->hw_data = pavo;

	return(pavo->vm);


	err_vm:
	free(pavo);
	return NULL;



}




int init_instance(vm_instance_t *vm)
{
	pavo_t *pavo=VM_PAVO(vm);


	if (pavo_init_platform(pavo) == -1) {
		vm_error(vm,"unable to initialize the platform hardware.\n");
		return(-1);
	}
	/* IRQ routing */
	vm->set_irq = pavo_set_irq;
	vm->clear_irq = pavo_clear_irq;

	return(pavo_boot(pavo));

}
void forced_inline virtual_pavo_timer(cpu_mips_t *cpu)
{

}
void forced_inline virtual_timer(cpu_mips_t *cpu)
{
	virtual_pavo_timer(cpu);
}

