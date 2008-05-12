 /*
   * Copyright (C) yajin 2008 <yajinzhou@gmail.com >
   *     
   * This file is part of the virtualmips distribution. 
   * See LICENSE file for terms of the license. 
   *
   */




#include "cpu.h"
#include "vm.h"
#include "mips64_exec.h"
#include "mips64_fdd.h"
#include "mips64_direct_threaded.h"
#include "vp_timer.h"

cpu_mips_t *current_cpu;







/*select different main loop*/
void * mips64_exec_run_cpu(cpu_mips_t * cpu)
{
#ifdef _USE_DIRECT_THREAED_
	if (mips64_cpu_direct_threaded_init(cpu)==-1)
		return NULL;
	return mips64_cpu_direct_threaded(cpu);
#endif

#ifdef  _USE_FDD_
	return mips64_cpu_fdd(cpu);
#endif

}








