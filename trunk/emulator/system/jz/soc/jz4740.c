


int jz4740_boot_from_nandflash(vm_instance_t *vm)
{

  cpu_mips_t *cpu=vm->boot_cpu;
  pavo_t *pavo;
  if (vm->type==VM_TYPE_PAVO)
    {
      pavo=VM_PAVO(vm);
    }
  else
    ASSERT(0,"Error vm type\n");

  
  
}
