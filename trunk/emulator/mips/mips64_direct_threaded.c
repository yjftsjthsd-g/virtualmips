/*
  * Copyright (C) yajin 2008 <yajinzhou@gmail.com >
  *     
  * This file is part of the virtualmips distribution. 
  * See LICENSE file for terms of the license. 
  *
  */

/*
MIPS64 direct threaded optimization
The idea beyond threaded optimization is to eliminate the main dispatch loop.
See Virtual Machine: versatile platform for systems and processes page 37.
yajin
*/

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <time.h>

#include "cpu.h"
#include "vm.h"
#include "mips64_exec.h"
#include "mips64_memory.h"
#include "ins_lookup.h"
#include "mips64.h"
#include "mips64_cp0.h"
#include "debug.h"
#include "vp_timer.h"
#include "mips64_hostalarm.h"

#ifdef _USE_DIRECT_THREAED_


/*threaded_code is a memory chunk contains the interrupt routine address.
*/
m_hiptr_t *threaded_code;


#define EXIST_FETCH_EXCEPTION  1 /*fetch instruction EXCEPTION*/
#define EXIST_CPU_PAUSE             2 /*Timer out. CPU Pause*/
#define EXIST_CPU_RESET             3 /*CPU RESET request*/

static struct mips64_op_desc mips_opcodes[];
static struct mips64_op_desc mips_spec_opcodes[];
static struct mips64_op_desc mips_bcond_opcodes[];
static struct mips64_op_desc mips_cop0_opcodes[];
static struct mips64_op_desc mips_mad_opcodes[];
static struct mips64_op_desc mips_tlb_opcodes[];

extern cpu_mips_t *current_cpu;


#ifdef DEBUG_MHZ
#define C_1000MHZ 1000000000
struct timeval pstart, pend;
float timeuse, performance;
m_uint64_t instructions_executed = 0;

#define get_mhz() \
do { \
   if (unlikely(instructions_executed == 0))  \
   { \
      gettimeofday(&pstart, NULL); \
   } \
   instructions_executed++; \
   if (unlikely(instructions_executed == C_1000MHZ)) \
   { \
      gettimeofday(&pend, NULL); \
      timeuse = 1000000 * (pend.tv_sec - pstart.tv_sec) + pend.tv_usec - pstart.tv_usec; \
      timeuse /= 1000000; \
      performance = 1000 / timeuse; \
      printf("Used Time:%f seconds.  %f MHZ\n", timeuse, performance); \
      exit(1); \
   }  \
} while (0)
#else
#define get_mhz() do {}while (0)
#endif

jmp_buf exit_point;

#define check_cpu_pause(cpu) \
	do {if (unlikely((cpu->pause_request) & CPU_INTERRUPT_EXIT)) \
      { \
         cpu->state = CPU_STATE_PAUSING; \
         longjmp(exit_point,EXIST_CPU_PAUSE); \
      }} while(0)  

#define check_cpu_interrupt(cpu) \
	do { if (unlikely(cpu->irq_pending))  \
      { \
         mips64_trigger_irq(cpu); \
      }} while(0)  


#define cpu_fetch_instruciton(cpu,insn) \
    do {  if (unlikely(mips64_fetch_instruction(cpu, cpu->pc, &insn)==1))  \
      { \
      	longjmp(exit_point,EXIST_FETCH_EXCEPTION); \
      }} while(0)  

#define cpu_update_pc(cpu,res) \
	do { \
		if (likely(!res)) \
			cpu->pc += sizeof(mips_insn_t);  \
	} while(0) 

#define cpu_dispatch_instruction(cpu,insn) \
   do { major_op = MAJOR_OP(insn); \
   			get_mhz(); \
   goto *mips_labelcodes[major_op].label; \
   }while(0) \


#define mips64_fetch_and_dispatch(cpu,insn) \
do { \
   exec_page = (cpu->pc) & ~(m_va_t) MIPS_MIN_PAGE_IMASK; \
   if (unlikely(exec_page != cpu->njm_exec_page)) \
   { \
      if ((cpu->translate(cpu,cpu->pc,&exec_guest_page))==-1)  \
      		longjmp(exit_point,EXIST_FETCH_EXCEPTION);   \ 
      cpu->njm_exec_page = exec_page;  \
      cpu->njm_exec_guest_page = exec_guest_page; \
      cpu->njm_exec_ptr = cpu->mem_op_lookup(cpu, exec_page); \
      if (cpu->njm_exec_ptr ==NULL)  \
      ASSERT (0,"FFF\n"); \
   } \
    code_ptr= threaded_code + (cpu->njm_exec_guest_page <<(MIPS_MIN_PAGE_SHIFT-2));  \
    code_ptr+= ((cpu->pc & MIPS_MIN_PAGE_IMASK) >> 2) ;  \
    insn = vmtoh32(cpu->njm_exec_ptr[(cpu->pc & MIPS_MIN_PAGE_IMASK) >> 2]);  \
	if (*code_ptr==0)     \
	{ \
		major_op = MAJOR_OP(insn) ; \
		*code_ptr=(m_hiptr_t)mips_labelcodes[major_op].label;   \
	} \
	get_mhz(); \
   goto **code_ptr;   \
} while(0)


/*#define cpu_next_instruction(cpu,res,insn) \
	do { cpu_update_pc(cpu,res); \
	check_cpu_pause(cpu); \
	check_cpu_interrupt(cpu); \
	cpu_fetch_instruciton(cpu,insn); \
	cpu_dispatch_instruction(cpu,insn); }while(0)
*/
#define cpu_next_instruction(cpu,res,insn) \
	do { cpu_update_pc(cpu,res); \
	check_cpu_pause(cpu); \
	check_cpu_interrupt(cpu); \
	mips64_fetch_and_dispatch(cpu,insn); \
}while(0)
void forced_inline mips64_main_loop_wait(cpu_mips_t * cpu, int timeout)
{
   vp_run_timers(&active_timers[VP_TIMER_REALTIME], vp_get_clock(rt_clock));
}

/* Execute a memory operation (2) */
int forced_inline mips64_exec_memop2(cpu_mips_t * cpu, int memop,
                                            m_va_t base, int offset, u_int dst_reg, int keep_ll_bit)
{
   m_va_t vaddr = cpu->gpr[base] + sign_extend(offset, 16);
   mips_memop_fn fn;

   if (!keep_ll_bit)
      cpu->ll_bit = 0;
   fn = cpu->mem_op_fn[memop];
   return (fn(cpu, vaddr, dst_reg));
}


/* Fetch an instruction */
static forced_inline int mips64_fetch_instruction(cpu_mips_t * cpu, m_va_t pc, mips_insn_t * insn)
{
   m_va_t exec_page;
   m_uint32_t offset;

   exec_page = pc & ~(m_va_t) MIPS_MIN_PAGE_IMASK;
   if (unlikely(exec_page != cpu->njm_exec_page))
   {
      cpu->njm_exec_page = exec_page;
      cpu->njm_exec_ptr = cpu->mem_op_lookup(cpu, exec_page);
   }

   if (cpu->njm_exec_ptr == NULL)
   {
      //exception when fetching instruction
      return (1);
   }

   offset = (pc & MIPS_MIN_PAGE_IMASK) >> 2;
   *insn = vmtoh32(cpu->njm_exec_ptr[offset]);
   /*
   Hi yajin, why you set  cpu->njm_exec_page to 0x1 here??
   803f2bffc	     	beqz 	v1,803f2d004 
	803f2c000:	 	addiu	v0,a0,258

	when emulate instruciton beqz, it will call mips64_exec_bdslot->mips64_fetch_instruction.
	If we do not set  cpu->njm_exec_page = 0x1,  cpu->njm_exec_page =exec_page, which is
	803f2c000.
	And then execute cpu_next_instruction->mips64_fetch_and_dispatch.
	
   */
   
   cpu->njm_exec_page = 0x1;
   return (0);

}


#if 0
/* Fetch and dispatch an instruction using directed threaded optimizaition*/
static forced_inline int mips64_fetch_and_dispatch(cpu_mips_t * cpu, m_va_t pc, mips_insn_t * insn)
{

	m_pa_t exec_guest_page;
   m_va_t exec_page;
   //m_uint32_t offset;
   m_uint8_t  * code_ptr;

   exec_page = pc & ~(m_va_t) MIPS_MIN_PAGE_IMASK;
   if (unlikely(exec_page != cpu->njm_exec_page))
   {
   		/*we are in another page. 
   		we use pc to determine whether last instruction and current instruction is in the same page.*/
      cpu->njm_exec_page = exec_page;
      if (cpu->translate(cpu,pc,&exec_guest_page)==-1)
      {
      		/*current instruction caused a TLB exception*/
      		return (1);
      }
      cpu->njm_exec_guest_page = exec_guest_page;
   }

   code_ptr= threaded_code + (((exec_guest_page<<MIPS_MIN_PAGE_SHIFT)+(pc &MIPS_MIN_PAGE_IMASK))>>2);
   if (code_ptr==NULL)
   	{
   		major_op = MAJOR_OP(insn)
   		*code_ptr = mips_labelcodes[major_op].label; 
   	}

   goto *code_ptr;
   
}
#endif

/* Execute a single instruction */
static forced_inline int mips64_exec_single_instruction(cpu_mips_t * cpu, mips_insn_t instruction)
{
   register uint op;
   op = MAJOR_OP(instruction);

   return mips_opcodes[op].func(cpu, instruction);
}

/* Execute the instruction in delay slot */
static forced_inline int mips64_exec_bdslot(cpu_mips_t * cpu)
{
   mips_insn_t insn;
   int res = 0;
   cpu->is_in_bdslot = 1;

   /* Fetch the instruction in delay slot */
   res = mips64_fetch_instruction(cpu, cpu->pc + 4, &insn);
   if (res == 1)
   {
      /*exception when fetching instruction */
      cpu->is_in_bdslot = 0;
      return (1);
   }
   cpu->is_in_bdslot = 1;
   /* Execute the instruction */
   res = mips64_exec_single_instruction(cpu, insn);
   cpu->is_in_bdslot = 0;
   return res;
}






int mips64_cpu_direct_threaded_init(cpu_mips_t * cpu)
{
	/*TODO: Currently we just alloc a memory chunk having a size of ramsize.
	Can be optimized.	   */
	m_uint32_t len;
	len = cpu->vm->ram_size*1024*1024*sizeof(m_hiptr_t )/sizeof(mips_insn_t);
	threaded_code= malloc(len);
	if (threaded_code==NULL)
	{
		fprintf(stderr,
              "mips64_cpu_direct_threaded_init: unable to create threaded code area.\n");
      return(-1);
	}
	memset(threaded_code,0,len);
	return (0);
      
}
	
	
void *mips64_cpu_direct_threaded(cpu_mips_t * cpu)
{
	register uint major_op;
	m_pa_t exec_guest_page;
   m_va_t exec_page;
   //m_uint32_t offset;
   m_hiptr_t * code_ptr;

	
	struct mips64_label_desc mips_labelcodes[] = {
   {"spec", &&spec_label, 0x00},
   {"bcond", &&bcond_label, 0x01},
   {"j", &&j_label, 0x02},
   {"jal", &&jal_label, 0x03},
   {"beq", &&beq_label, 0x04},
   {"bne", &&bne_label, 0x05},
   {"blez", &&blez_label, 0x06},
   {"bgtz", &&bgtz_label, 0x07},
   {"addi", &&addi_label, 0x08},
   {"addiu", &&addiu_label, 0x09},
   {"slti", &&slti_label, 0x0A},
   {"sltiu", &&sltiu_label, 0x0B},
   {"andi",&&andi_label, 0x0C},
   {"ori", &&ori_label, 0x0D},
   {"xori", &&xori_label, 0x0E},
   {"lui", &&lui_label, 0x0F},
   {"cop0", &&cop0_label, 0x10},
   {"cop1", &&cop1_label, 0x11},
   {"cop2", &&cop2_label, 0x12},
   {"cop1x", &&cop1x_label, 0x13},
   {"beql", &&beql_label, 0x14},
   {"bnel", &&bnel_label, 0x15},
   {"blezl", &&blezl_label, 0x16},
   {"bgtzl", &&bgtzl_label, 0x17},
   {"daddi", &&daddi_label, 0x18},
   {"daddiu", &&daddiu_label, 0x19},
   {"ldl", &&ldl_label, 0x1A},
   {"ldr", &&ldr_label, 0x1B},
   {"undef", &&mad_label, 0x1C},
   {"undef", &&undef_label, 0x1D},
   {"undef", &&undef_label, 0x1E},
   {"undef", &&undef_label, 0x1F},
   {"lb", &&lb_label, 0x20},
   {"lh", &&lh_label, 0x21},
   {"lwl", &&lwl_label, 0x22},
   {"lw", &&lw_label, 0x23},
   {"lbu", &&lbu_label, 0x24},
   {"lhu", &&lhu_label, 0x25},
   {"lwr", &&lwr_label, 0x26},
   {"lwu", &&lwu_label, 0x27},
   {"sb", &&sb_label, 0x28},
   {"sh", &&sh_label, 0x29},
   {"swl", &&swl_label, 0x2A},
   {"sw", &&sw_label, 0x2B},
   {"sdl", &&sdl_label, 0x2C},
   {"sdr", &&sdr_label, 0x2D},
   {"swr", &&swr_label, 0x2E},
   {"cache", &&cache_label, 0x2F},
   {"ll", &&ll_label, 0x30},
   {"lwc1", &&lwc1_label, 0x31},
   {"lwc2", &&lwc2_label, 0x32},
   {"pref", &&pref_label, 0x33},
   {"lld", &&lld_label, 0x34},
   {"ldc1", &&ldc1_label, 0x35},
   {"ldc2", &&ldc2_label, 0x36},
   {"ld", &&ld_label, 0x37},
   {"sc", &&sc_label, 0x38},
   {"swc1", &&swc1_label, 0x39},
   {"swc2", &&swc2_label, 0x3A},
   {"undef", &&undef_label, 0x3B},
   {"scd", &&scd_label, 0x3C},
   {"sdc1", &&sdc1_label, 0x3D},
   {"sdc2", &&sdc2_label, 0x3E},
   {"sd", &&sd_label, 0x3F},
};

/* Based on the func field of spec opcode */
static struct mips64_label_desc mips_spec_labelcodes[] = {
   {"sll", &&sll_label, 0x00},
   {"movc", &&movc_label, 0x01},
   {"srl", &&srl_label, 0x02},
   {"sra", &&sra_label, 0x03},
   {"sllv", &&sllv_label, 0x04},
   {"unknownSpec", &&unknownSpeclabel, 0x05},
   {"srlv", &&srlv_label, 0x06},
   {"srav", &&srav_label, 0x07},
   {"jr", &&jr_label, 0x08},
   {"jalr", &&jalr_label, 0x09},
   {"movz", &&movz_label, 0x0A},
   {"movn", &&movn_label, 0x0B},
   {"syscall", &&syscall_label, 0x0C},
   {"break", &&break_label, 0x0D},
   {"spim", &&unknownSpeclabel, 0x0E},
   {"sync", &&sync_label, 0x0F},
   {"mfhi", &&mfhi_label, 0x10},
   {"mthi", &&mthi_label, 0x11},
   {"mflo", &&mflo_label, 0x12},
   {"mtlo", &&mtlo_label, 0x13},
   {"dsllv", &&dsllv_label, 0x14},
   {"unknownSpec", &&unknownSpeclabel, 0x15},
   {"dsrlv", &&dsrlv_label, 0x16},
   {"dsrav", &&dsrav_label, 0x17},
   {"mult", &&mult_label, 0x18},
   {"multu", &&multu_label, 0x19},
   {"div", &&div_label, 0x1A},
   {"divu", &&divu_label, 0x1B},
   {"dmult", &&dmult_label, 0x1C},
   {"dmultu", &&dmultu_label, 0x1D},
   {"ddiv", &&ddiv_label, 0x1E},
   {"ddivu", &&ddivu_label, 0x1F},
   {"add", &&add_label, 0x20},
   {"addu", &&addu_label, 0x21},
   {"sub", &&sub_label, 0x22},
   {"subu", &&subu_label, 0x23},
   {"and", &&and_label, 0x24},
   {"or", &&or_label, 0x25},
   {"xor", &&xor_label, 0x26},
   {"nor", &&nor_label, 0x27},
   {"unknownSpec", &&unknownSpeclabel, 0x28},
   {"unknownSpec", &&unknownSpeclabel, 0x29},
   {"slt", &&slt_label, 0x2A},
   {"sltu", &&sltu_label, 0x2B},
   {"dadd", &&dadd_label, 0x2C},
   {"daddu", &&daddu_label, 0x2D},
   {"dsub", &&dsub_label, 0x2E},
   {"dsubu", &&dsubu_label, 0x2F},
   {"tge", &&tge_label, 0x30},
   {"tgeu", &&tgeu_label, 0x31},
   {"tlt", &&tlt_label, 0x32},
   {"tltu", &&tltu_label, 0x33},
   {"teq", &&teq_label, 0x34},
   {"unknownSpec", &&unknownSpeclabel, 0x35},
   {"tne", &&tne_label, 0x36},
   {"unknownSpec", &&unknownSpeclabel, 0x37},
   {"dsll", &&dsll_label, 0x38},
   {"unknownSpec", &&unknownSpeclabel, 0x39},
   {"dsrl", &&dsrl_label, 0x3A},
   {"dsra", &&dsra_label, 0x3B},
   {"dsll32", &&dsll32_label, 0x3C},
   {"unknownSpec", &&unknownSpeclabel, 0x3D},
   {"dsrl32", &&dsrl32_label, 0x3E},
   {"dsra32", &&dsra32_label, 0x3F}
};








/* Based on the rt field of bcond opcodes */
 struct mips64_label_desc mips_bcond_labelcodes[] = {
   {"bltz", &&bltz_label, 0x00},
   {"bgez", &&bgez_label, 0x01},
   {"bltzl", &&bltzl_label, 0x02},
   {"bgezl", &&bgezl_label, 0x03},
   {"spimi", &&unknownBcondlabel, 0x04},
   {"unknownBcond", &&unknownBcondlabel, 0x05},
   {"unknownBcond", &&unknownBcondlabel, 0x06},
   {"unknownBcond", &&unknownBcondlabel, 0x07},
   {"tgei", &&tgei_label, 0x08},
   {"tgeiu", &&tgeiu_label, 0x09},
   {"tlti", &&tlti_label, 0x0A},
   {"tltiu", &&tltiu_label, 0x0B},
   {"teqi", &&teqi_label, 0x0C},
   {"unknownBcond", &&unknownBcondlabel, 0x0D},
   {"tnei", &&tnei_label, 0x0E},
   {"unknownBcond", &&unknownBcondlabel, 0x0F},
   {"bltzal", &&bltzal_label, 0x10},
   {"bgezal", &&bgezal_label, 0x11},
   {"bltzall", &&bltzall_label, 0x12},
   {"bgezall", &&bgezall_label, 0x13},
   {"unknownBcond", &&unknownBcondlabel, 0x14},
   {"unknownBcond", &&unknownBcondlabel, 0x15},
   {"unknownBcond", &&unknownBcondlabel, 0x16},
   {"unknownBcond", &&unknownBcondlabel, 0x17},
   {"unknownBcond", &&unknownBcondlabel, 0x18},
   {"unknownBcond", &&unknownBcondlabel, 0x19},
   {"unknownBcond", &&unknownBcondlabel, 0x1A},
   {"unknownBcond", &&unknownBcondlabel, 0x1B},
   {"unknownBcond", &&unknownBcondlabel, 0x1C},
   {"unknownBcond", &&unknownBcondlabel, 0x1D},
   {"unknownBcond", &&unknownBcondlabel, 0x1E},
   {"unknownBcond", &&unknownBcondlabel, 0x1F}
};


 struct mips64_label_desc mips_cop0_labelcodes[] = {
   {"mfc0", &&mfc0_label, 0x0},
   {"dmfc0", &&dmfc0_label, 0x1},
   {"cfc0", &&cfc0_label, 0x2},
   {"unknowncop0", &&unknowncop0_label, 0x3},
   {"mtc0", &&mtc0_label, 0x4},
   {"dmtc0", &&dmtc0_label, 0x5},
   {"unknowncop0", &&unknowncop0_label, 0x6},
   {"unknowncop0", &&unknowncop0_label, 0x7},
   {"unknowncop0", &&unknowncop0_label, 0x8},
   {"unknowncop0", &&unknowncop0_label, 0x9},
   {"unknowncop0", &&unknowncop0_label, 0xa},
   {"unknowncop0", &&unknowncop0_label, 0xb},
   {"unknowncop0", &&unknowncop0_label, 0xc},
   {"unknowncop0", &&unknowncop0_label, 0xd},
   {"unknowncop0", &&unknowncop0_label, 0xe},
   {"unknowncop0", &&unknowncop0_label, 0xf},
   {"tlb", &&tlb_label, 0x10},
   {"unknowncop0", &&unknowncop0_label, 0x11},
   {"unknowncop0", &&unknowncop0_label, 0x12},
   {"unknowncop0", &&unknowncop0_label, 0x13},
   {"unknowncop0", &&unknowncop0_label, 0x14},
   {"unknowncop0", &&unknowncop0_label, 0x15},
   {"unknowncop0", &&unknowncop0_label, 0x16},
   {"unknowncop0", &&unknowncop0_label, 0x17},
   {"unknowncop0", &&unknowncop0_label, 0x18},
   {"unknowncop0", &&unknowncop0_label, 0x19},
   {"unknowncop0", &&unknowncop0_label, 0x1a},
   {"unknowncop0", &&unknowncop0_label, 0x1b},
   {"unknowncop0", &&unknowncop0_label, 0x1c},
   {"unknowncop0", &&unknowncop0_label, 0x1d},
   {"unknowncop0", &&unknowncop0_label, 0x1e},
   {"unknowncop0", &&unknowncop0_label, 0x1f},

};



 struct mips64_label_desc mips_mad_labelcodes[] = {
   {"mad", &&madd_label, 0x0},
   {"maddu", &&maddu_label, 0x1},
   {"mul", &&mul_label, 0x2},
   {"unknownmad_op", &&unknownmad_label, 0x3},
   {"msub", &&msub_label, 0x4},
   {"msubu", &&msubu_label, 0x5},
   {"unknownmad_op", &&unknownmad_label, 0x6},
   {"unknownmad_op", &&unknownmad_label, 0x7},
   {"unknownmad_op", &&unknownmad_label, 0x8},
   {"unknownmad_op", &&unknownmad_label, 0x9},
   {"unknownmad_op", &&unknownmad_label, 0xa},
   {"unknownmad_op", &&unknownmad_label, 0xb},
   {"unknownmad_op", &&unknownmad_label, 0xc},
   {"unknownmad_op", &&unknownmad_label, 0xd},
   {"unknownmad_op", &&unknownmad_label, 0xe},
   {"unknownmad_op", &&unknownmad_label, 0xf},
   {"unknownmad_op", &&unknownmad_label, 0x10},
   {"unknownmad_op", &&unknownmad_label, 0x11},
   {"unknownmad_op", &&unknownmad_label, 0x12},
   {"unknownmad_op", &&unknownmad_label, 0x13},
   {"unknownmad_op", &&unknownmad_label, 0x14},
   {"unknownmad_op", &&unknownmad_label, 0x15},
   {"unknownmad_op", &&unknownmad_label, 0x16},
   {"unknownmad_op", &&unknownmad_label, 0x17},
   {"unknownmad_op", &&unknownmad_label, 0x18},
   {"unknownmad_op", &&unknownmad_label, 0x19},
   {"unknownmad_op", &&unknownmad_label, 0x1a},
   {"unknownmad_op", &&unknownmad_label, 0x1b},
   {"unknownmad_op", &&unknownmad_label, 0x1c},
   {"unknownmad_op", &&unknownmad_label, 0x1d},
   {"unknownmad_op", &&unknownmad_label, 0x1e},
   {"unknownmad_op", &&unknownmad_label, 0x1f},
   {"clz", &&clz_label, 0x20},
   {"unknownmad_op", &&unknownmad_label, 0x21},
   {"unknownmad_op", &&unknownmad_label, 0x22},
   {"unknownmad_op", &&unknownmad_label, 0x23},
   {"unknownmad_op", &&unknownmad_label, 0x24},
   {"unknownmad_op", &&unknownmad_label, 0x25},
   {"unknownmad_op", &&unknownmad_label, 0x26},
   {"unknownmad_op", &&unknownmad_label, 0x27},
   {"unknownmad_op", &&unknownmad_label, 0x28},
   {"unknownmad_op", &&unknownmad_label, 0x29},
   {"unknownmad_op", &&unknownmad_label, 0x2a},
   {"unknownmad_op", &&unknownmad_label, 0x2b},
   {"unknownmad_op", &&unknownmad_label, 0x2c},
   {"unknownmad_op", &&unknownmad_label, 0x2d},
   {"unknownmad_op", &&unknownmad_label, 0x2e},
   {"unknownmad_op", &&unknownmad_label, 0x2f},
   {"unknownmad_op", &&unknownmad_label, 0x30},
   {"unknownmad_op", &&unknownmad_label, 0x31},
   {"unknownmad_op", &&unknownmad_label, 0x32},
   {"unknownmad_op", &&unknownmad_label, 0x33},
   {"unknownmad_op", &&unknownmad_label, 0x34},
   {"unknownmad_op", &&unknownmad_label, 0x35},
   {"unknownmad_op", &&unknownmad_label, 0x36},
   {"unknownmad_op", &&unknownmad_label, 0x37},
   {"unknownmad_op", &&unknownmad_label, 0x38},
   {"unknownmad_op", &&unknownmad_label, 0x39},
   {"unknownmad_op", &&unknownmad_label, 0x3a},
   {"unknownmad_op", &&unknownmad_label, 0x3b},
   {"unknownmad_op", &&unknownmad_label, 0x3c},
   {"unknownmad_op", &&unknownmad_label, 0x3d},
   {"unknownmad_op", &&unknownmad_label, 0x3e},
   {"unknownmad_op", &&unknownmad_label, 0x3f},

};


 struct mips64_label_desc mips_tlb_labelcodes[] = {
   {"unknowntlb_op", &&unknowntlb_label, 0x0},
   {"tlbr", &&tlbr_label, 0x1},
   {"tlbwi", &&tlbwi_label, 0x2},
   {"unknowntlb_op", &&unknowntlb_label, 0x3},
   {"unknowntlb_op", &&unknowntlb_label, 0x4},
   {"unknowntlb_op", &&unknowntlb_label, 0x5},
   {"tlbwi", &&tlbwr_label, 0x6},
   {"unknowntlb_op", &&unknowntlb_label, 0x7},
   {"tlbp", &&tlbp_label, 0x8},
   {"unknowntlb_op", &&unknowntlb_label, 0x9},
   {"unknowntlb_op", &&unknowntlb_label, 0xa},
   {"unknowntlb_op", &&unknowntlb_label, 0xb},
   {"unknowntlb_op", &&unknowntlb_label, 0xc},
   {"unknowntlb_op", &&unknowntlb_label, 0xd},
   {"unknowntlb_op", &&unknowntlb_label, 0xe},
   {"unknowntlb_op",&&unknowntlb_label, 0xf},
   {"unknowntlb_op", &&unknowntlb_label, 0x10},
   {"unknowntlb_op", &&unknowntlb_label, 0x11},
   {"unknowntlb_op", &&unknowntlb_label, 0x12},
   {"unknowntlb_op", &&unknowntlb_label, 0x13},
   {"unknowntlb_op", &&unknowntlb_label, 0x14},
   {"unknowntlb_op", &&unknowntlb_label, 0x15},
   {"unknowntlb_op", &&unknowntlb_label, 0x16},
   {"unknowntlb_op", &&unknowntlb_label, 0x17},
   {"eret", &&eret_label, 0x18},
   {"unknowntlb_op", &&unknowntlb_label, 0x19},
   {"unknowntlb_op", &&unknowntlb_label, 0x1a},
   {"unknowntlb_op", &&unknowntlb_label, 0x1b},
   {"unknowntlb_op", &&unknowntlb_label, 0x1c},
   {"unknowntlb_op", &&unknowntlb_label, 0x1d},
   {"unknowntlb_op", &&unknowntlb_label, 0x1e},
   {"unknowntlb_op", &&unknowntlb_label, 0x1f},
   {"wait", &&wait_label, 0x20},
   {"unknowntlb_op", &&unknowntlb_label, 0x21},
   {"unknowntlb_op", &&unknowntlb_label, 0x22},
   {"unknowntlb_op", &&unknowntlb_label, 0x23},
   {"unknowntlb_op", &&unknowntlb_label, 0x24},
   {"unknowntlb_op", &&unknowntlb_label, 0x25},
   {"unknowntlb_op", &&unknowntlb_label, 0x26},
   {"unknowntlb_op", &&unknowntlb_label, 0x27},
   {"unknowntlb_op", &&unknowntlb_label, 0x28},
   {"unknowntlb_op", &&unknowntlb_label, 0x29},
   {"unknowntlb_op", &&unknowntlb_label, 0x2a},
   {"unknowntlb_op", &&unknowntlb_label, 0x2b},
   {"unknowntlb_op", &&unknowntlb_label, 0x2c},
   {"unknowntlb_op", &&unknowntlb_label, 0x2d},
   {"unknowntlb_op", &&unknowntlb_label, 0x2e},
   {"unknowntlb_op", &&unknowntlb_label, 0x2f},
   {"unknowntlb_op", &&unknowntlb_label, 0x30},
   {"unknowntlb_op", &&unknowntlb_label, 0x31},
   {"unknowntlb_op", &&unknowntlb_label, 0x32},
   {"unknowntlb_op", &&unknowntlb_label, 0x33},
   {"unknowntlb_op", &&unknowntlb_label, 0x34},
   {"unknowntlb_op", &&unknowntlb_label, 0x35},
   {"unknowntlb_op", &&unknowntlb_label, 0x36},
   {"unknowntlb_op", &&unknowntlb_label, 0x37},
   {"unknowntlb_op", &&unknowntlb_label, 0x38},
   {"unknowntlb_op", &&unknowntlb_label, 0x39},
   {"unknowntlb_op", &&unknowntlb_label, 0x3a},
   {"unknowntlb_op", &&unknowntlb_label, 0x3b},
   {"unknowntlb_op", &&unknowntlb_label, 0x3c},
   {"unknowntlb_op", &&unknowntlb_label, 0x3d},
   {"unknowntlb_op", &&unknowntlb_label, 0x3e},
   {"unknowntlb_op", &&unknowntlb_label, 0x3f},

};

	mips_insn_t insn = 0;
	int exit_reson;
	
	cpu->cpu_thread_running = TRUE;
   current_cpu = cpu; 

   mips64_init_host_alarm();
  

 
   exit_reson=setjmp(exit_point);
  	 /*Set longjmp point. */
  	if (exit_reson==0)
  	{
  		printf("setjmp successed\n");
  	}
  	else
  	{
  		if (exit_reson==EXIST_CPU_PAUSE)
  		{
  				/*exist from normal excution*/
  	  	if (unlikely((cpu->pause_request) & CPU_INTERRUPT_EXIT))
      	{
         cpu->state = CPU_STATE_PAUSING;
         /*main loop must wait for me. heihei :) */
         mips64_main_loop_wait(cpu, 0);
         cpu->state = CPU_STATE_RUNNING;
         cpu->pause_request &= ~CPU_INTERRUPT_EXIT;
      	}
  	 }
    else if (exit_reson==EXIST_FETCH_EXCEPTION)
    {
    	/*just goto main loop again*/
    }
    else if (exit_reson==EXIST_CPU_RESET)
    {
    	/*Do nothing.*/
    }
  	

  		
  	}

/*waiting for CPU Running*/
while (cpu->state != CPU_STATE_RUNNING);


/*OK CPU is running.*/
check_cpu_pause(cpu);
mips64_fetch_and_dispatch(cpu,insn); 
//cpu_fetch_instruciton(cpu,insn);
//cpu_dispatch_instruction(cpu,insn);

unknown_lable:
	printf("unknown instruction. pc %x insn %x\n", cpu->pc, insn);
   exit(EXIT_FAILURE);
add_label:
{
	int rs_add,rt_add,rd_add;
	m_reg_t res_add;
   rs_add= bits(insn, 21, 25);
   rt_add = bits(insn, 16, 20);
   rd_add = bits(insn, 11, 15);

   /* TODO: Exception handling */
   res_add = (m_reg_t) (m_uint32_t) cpu->gpr[rs_add] + (m_uint32_t) cpu->gpr[rt_add];
   cpu->gpr[rd_add] = sign_extend(res_add, 32);
   /*fetch next instruction*/
   cpu_next_instruction(cpu,0,insn);
}
addi_label:
{
   int rs_addi = bits(insn, 21, 25);
   int rt_addi = bits(insn, 16, 20);
   int imm_addi = bits(insn, 0, 15);
   m_uint32_t res_addi, val_addi = sign_extend(imm_addi, 16);

   /* TODO: Exception handling */
   res_addi = (m_uint32_t) cpu->gpr[rs_addi] + val_addi;
   cpu->gpr[rt_addi] = sign_extend(res_addi, 32);
   cpu_next_instruction(cpu,0,insn);
}
addiu_label:
{
   int rs_addiu = bits(insn, 21, 25);
   int rt_addiu = bits(insn, 16, 20);
   int imm_addiu = bits(insn, 0, 15);
   m_uint32_t res_addiu, val_addiu = sign_extend(imm_addiu, 16);

   res_addiu = (m_uint32_t) cpu->gpr[rs_addiu] + val_addiu;
   cpu->gpr[rt_addiu] = sign_extend(res_addiu, 32);
   cpu_next_instruction(cpu,0,insn);
}
addu_label:
{
   int rs_addu = bits(insn, 21, 25);
   int rt_addu = bits(insn, 16, 20);
   int rd_addu = bits(insn, 11, 15);
   m_uint32_t res_addu;

   res_addu = (m_uint32_t) cpu->gpr[rs_addu] + (m_uint32_t) cpu->gpr[rt_addu];
   cpu->gpr[rd_addu] = sign_extend(res_addu, 32);
   cpu_next_instruction(cpu,0,insn);
}
and_label:
{
   int rs_and = bits(insn, 21, 25);
   int rt_and = bits(insn, 16, 20);
   int rd_and = bits(insn, 11, 15);

   cpu->gpr[rd_and] = cpu->gpr[rs_and] & cpu->gpr[rt_and];
   cpu_next_instruction(cpu,0,insn);
}

andi_label:
{
   int rs_andi = bits(insn, 21, 25);
   int rt_andi = bits(insn, 16, 20);
   int imm_andi = bits(insn, 0, 15);

   cpu->gpr[rt_andi] = cpu->gpr[rs_andi] & imm_andi;
   cpu_next_instruction(cpu,0,insn);
}
bcond_label:
{
   uint16_t special_func_bcond = bits(insn, 16, 20);
   goto  *mips_bcond_labelcodes[special_func_bcond].label;
}
beq_label:
{
   int rs_beq = bits(insn, 21, 25);
   int rt_beq = bits(insn, 16, 20);
   int offset_beq = bits(insn, 0, 15);
   m_va_t new_pc_beq;
   int res_beq;

   /* compute the new pc */
   new_pc_beq = (cpu->pc + 4) + sign_extend(offset_beq << 2, 18);

   /* take the branch if gpr[rs] == gpr[rt] */
   res_beq = (cpu->gpr[rs_beq] == cpu->gpr[rt_beq]);

   /* exec the instruction in the delay slot */
   int ins_res_beq = mips64_exec_bdslot(cpu);
  if (likely(!ins_res_beq))
   {
      if (res_beq)
         cpu->pc = new_pc_beq;
      else
         cpu->pc += 8;
   }

  cpu_next_instruction(cpu,1,insn);
}

beql_label:
{
   int rs_beql = bits(insn, 21, 25);
   int rt_beql = bits(insn, 16, 20);
   int offset_beql = bits(insn, 0, 15);
   m_va_t new_pc_beql;
   int res_beql;

   /* compute the new pc */
   new_pc_beql = (cpu->pc + 4) + sign_extend(offset_beql << 2, 18);

   res_beql = (cpu->gpr[rs_beql] == cpu->gpr[rt_beql]);

   /* take the branch if the test result is true */
   if (res_beql)
   {
      int ins_res_beql = mips64_exec_bdslot(cpu);
      if (likely(!ins_res_beql))
         cpu->pc = new_pc_beql;
   }
   else
      cpu->pc += 8;

   cpu_next_instruction(cpu,1,insn);
}
bgez_label:
{
   int rs_bgez = bits(insn, 21, 25);
   int offset_bgez = bits(insn, 0, 15);
   m_va_t new_pc_bgez;
   int res_bgez;

   /* compute the new pc */
   new_pc_bgez = (cpu->pc + 4) + sign_extend(offset_bgez << 2, 18);

   /* take the branch if gpr[rs] >= 0 */
   res_bgez = ((m_ireg_t) cpu->gpr[rs_bgez] >= 0);

   /* exec the instruction in the delay slot */
   /* exec the instruction in the delay slot */
   int ins_res_bgez = mips64_exec_bdslot(cpu);

   if (likely(!ins_res_bgez))
   {
      /* take the branch if the test result is true */
      if (res_bgez)
         cpu->pc = new_pc_bgez;
      else
         cpu->pc += 8;
   }


  cpu_next_instruction(cpu,1,insn);
}
bgezal_label:
{
   int rs_bgezal = bits(insn, 21, 25);
   int offset_bgezal = bits(insn, 0, 15);
   m_va_t new_pc_bgezal;
   int res_bgezal;

   /* compute the new pc */
   new_pc_bgezal = (cpu->pc + 4) + sign_extend(offset_bgezal << 2, 18);

   /* set the return address (instruction after the delay slot) */
   cpu->gpr[MIPS_GPR_RA] = cpu->pc + 8;

   /* take the branch if gpr[rs] >= 0 */
   res_bgezal = ((m_ireg_t) cpu->gpr[rs_bgezal] >= 0);

   /* exec the instruction in the delay slot */
   int ins_res_bgezal = mips64_exec_bdslot(cpu);

   if (likely(!ins_res_bgezal))

   {
      /* take the branch if the test result is true */
      if (res_bgezal)
         cpu->pc = new_pc_bgezal;
      else
         cpu->pc += 8;
   }
  cpu_next_instruction(cpu,1,insn);
}
bgezall_label:
{
   int rs_bgezall = bits(insn, 21, 25);
   int offset_bgezall = bits(insn, 0, 15);
   m_va_t new_pc_bgezall;
   int res_bgezall;

   /* compute the new pc */
   new_pc_bgezall = (cpu->pc + 4) + sign_extend(offset_bgezall << 2, 18);

   /* set the return address (instruction after the delay slot) */
   cpu->gpr[MIPS_GPR_RA] = cpu->pc + 8;

   /* take the branch if gpr[rs] >= 0 */
   res_bgezall = ((m_ireg_t) cpu->gpr[rs_bgezall] >= 0);

   /* take the branch if the test result is true */
   if (res_bgezall)
   {
      mips64_exec_bdslot(cpu);
      cpu->pc = new_pc_bgezall;
   }
   else
      cpu->pc += 8;

   cpu_next_instruction(cpu,1,insn);

}
bgezl_label:
{
   int rs_bgezl = bits(insn, 21, 25);
   int offset_bgezl = bits(insn, 0, 15);
   m_va_t new_pc_bgezl;
   int res_bgezl;

   /* compute the new pc */
   new_pc_bgezl = (cpu->pc + 4) + sign_extend(offset_bgezl << 2, 18);

   /* take the branch if gpr[rs] >= 0 */
   res_bgezl = ((m_ireg_t) cpu->gpr[rs_bgezl] >= 0);

   /* take the branch if the test result is true */
   if (res_bgezl)
   {
      mips64_exec_bdslot(cpu);
      cpu->pc = new_pc_bgezl;
   }
   else
      cpu->pc += 8;
   cpu_next_instruction(cpu,1,insn);
}
bgtz_label:
{
   int rs_bgtz = bits(insn, 21, 25);
   int offset_bgtz = bits(insn, 0, 15);
   m_va_t new_pc_bgtz;
   int res_bgtz;

   /* compute the new pc */
   new_pc_bgtz = (cpu->pc + 4) + sign_extend(offset_bgtz << 2, 18);

   /* take the branch if gpr[rs] > 0 */
   res_bgtz = ((m_ireg_t) cpu->gpr[rs_bgtz] > 0);

   /* exec the instruction in the delay slot */
   int ins_res_bgtz = mips64_exec_bdslot(cpu);

   if (likely(!ins_res_bgtz))
   {
      /* take the branch if the test result is true */
      if (res_bgtz)
         cpu->pc = new_pc_bgtz;
      else
         cpu->pc += 8;
   }

   cpu_next_instruction(cpu,1,insn);

}
bgtzl_label:
{
   int rs_bgtzl = bits(insn, 21, 25);
   int offset_bgtzl = bits(insn, 0, 15);
   m_va_t new_pc_bgtzl;
   int res_bgtzl;

   /* compute the new pc */
   new_pc_bgtzl = (cpu->pc + 4) + sign_extend(offset_bgtzl << 2, 18);

   /* take the branch if gpr[rs] > 0 */
   res_bgtzl = ((m_ireg_t) cpu->gpr[rs_bgtzl] > 0);

   /* take the branch if the test result is true */
   if (res_bgtzl)
   {
      mips64_exec_bdslot(cpu);
      cpu->pc = new_pc_bgtzl;
   }
   else
      cpu->pc += 8;

  cpu_next_instruction(cpu,1,insn);
}

blez_label:
	
{
   int rs_blez = bits(insn, 21, 25);
   int offset_blez = bits(insn, 0, 15);
   m_va_t new_pc_blez;
   int res_blez;

   /* compute the new pc */
   new_pc_blez = (cpu->pc + 4) + sign_extend(offset_blez << 2, 18);

   /* take the branch if gpr[rs] <= 0 */
   res_blez = ((m_ireg_t) cpu->gpr[rs_blez] <= 0);

   /* exec the instruction in the delay slot */
   int ins_res_blez = mips64_exec_bdslot(cpu);

   if (likely(!ins_res_blez))
   {
      /* take the branch if the test result is true */
      if (res_blez)
         cpu->pc = new_pc_blez;
      else
         cpu->pc += 8;
   }

  cpu_next_instruction(cpu,1,insn);

}
blezl_label:
{
   int rs_blezl = bits(insn, 21, 25);
   int offset_blezl = bits(insn, 0, 15);
   m_va_t new_pc_blezl;
   int res_blezl;

   /* compute the new pc */
   new_pc_blezl = (cpu->pc + 4) + sign_extend(offset_blezl << 2, 18);

   /* take the branch if gpr[rs] <= 0 */
   res_blezl = ((m_ireg_t) cpu->gpr[rs_blezl] <= 0);

   /* take the branch if the test result is true */
   if (res_blezl)
   {
      mips64_exec_bdslot(cpu);
      cpu->pc = new_pc_blezl;
   }
   else
      cpu->pc += 8;

   cpu_next_instruction(cpu,1,insn);
}

bltz_label:
{
   int rs_bltz = bits(insn, 21, 25);
   int offset_bltz = bits(insn, 0, 15);
   m_va_t new_pc_bltz;
   int res_bltz;

   /* compute the new pc */
   new_pc_bltz = (cpu->pc + 4) + sign_extend(offset_bltz << 2, 18);

   /* take the branch if gpr[rs] < 0 */
   res_bltz = ((m_ireg_t) cpu->gpr[rs_bltz] < 0);

   /* exec the instruction in the delay slot */
   int ins_res_bltz = mips64_exec_bdslot(cpu);

   if (likely(!ins_res_bltz))

   {
      /* take the branch if the test result is true */
      if (res_bltz)
         cpu->pc = new_pc_bltz;
      else
         cpu->pc += 8;

   }

  cpu_next_instruction(cpu,1,insn);

}
bltzal_label:
{
   int rs_bltzal = bits(insn, 21, 25);
   int offset_bltzal = bits(insn, 0, 15);
   m_va_t new_pc_bltzal;
   int res_bltzal;

   /* compute the new pc */
   new_pc_bltzal = (cpu->pc + 4) + sign_extend(offset_bltzal << 2, 18);

   /* set the return address (instruction after the delay slot) */
   cpu->gpr[MIPS_GPR_RA] = cpu->pc + 8;

   /* take the branch if gpr[rs] < 0 */
   res_bltzal = ((m_ireg_t) cpu->gpr[rs_bltzal] < 0);

   /* exec the instruction in the delay slot */
   int ins_res_bltzal = mips64_exec_bdslot(cpu);

   if (likely(!ins_res_bltzal))
   {
      /* take the branch if the test result is true */
      if (res_bltzal)
         cpu->pc = new_pc_bltzal;
      else
         cpu->pc += 8;
   }

   cpu_next_instruction(cpu,1,insn);


}
bltzall_label:
{
   int rs_bltzall = bits(insn, 21, 25);
   int offset_bltzall = bits(insn, 0, 15);
   m_va_t new_pc_bltzall;
   int res_bltzall;

   /* compute the new pc */
   new_pc_bltzall = (cpu->pc + 4) + sign_extend(offset_bltzall << 2, 18);

   /* set the return address (instruction after the delay slot) */
   cpu->gpr[MIPS_GPR_RA] = cpu->pc + 8;

   /* take the branch if gpr[rs] < 0 */
   res_bltzall = ((m_ireg_t) cpu->gpr[rs_bltzall] < 0);

   /* take the branch if the test result is true */
   if (res_bltzall)
   {
      mips64_exec_bdslot(cpu);
      cpu->pc = new_pc_bltzall;
   }
   else
      cpu->pc += 8;

  cpu_next_instruction(cpu,1,insn);
}


bltzl_label:
	
{




   int rs_bltzl = bits(insn, 21, 25);
   int offset_bltzl = bits(insn, 0, 15);
   m_va_t new_pc_bltzl;
   int res_bltzl;

   /* compute the new pc */
   new_pc_bltzl = (cpu->pc + 4) + sign_extend(offset_bltzl << 2, 18);

   /* take the branch if gpr[rs] < 0 */
   res_bltzl = ((m_ireg_t) cpu->gpr[rs_bltzl] < 0);

   /* take the branch if the test result is true */
   if (res_bltzl)
   {
      mips64_exec_bdslot(cpu);
      cpu->pc = new_pc_bltzl;
   }
   else
      cpu->pc += 8;

  cpu_next_instruction(cpu,1,insn);

}

bne_label:
{
   int rs_bne = bits(insn, 21, 25);
   int rt_bne = bits(insn, 16, 20);
   int offset_bne = bits(insn, 0, 15);
   m_va_t new_pc_bne;
   int res_bne;

   /* compute the new pc */
   new_pc_bne = (cpu->pc + 4) + sign_extend(offset_bne << 2, 18);


   /* take the branch if gpr[rs] != gpr[rt] */
   res_bne = (cpu->gpr[rs_bne] != cpu->gpr[rt_bne]);

   /* exec the instruction in the delay slot */
   int ins_res_bne = mips64_exec_bdslot(cpu);

   if (likely(!ins_res_bne))
   {
      /* take the branch if the test result is true */
      if (res_bne)
         cpu->pc = new_pc_bne;
      else
         cpu->pc += 8;
   }

   cpu_next_instruction(cpu,1,insn);
}
bnel_label:
{
   int rs_bnel = bits(insn, 21, 25);
   int rt_bnel = bits(insn, 16, 20);
   int offset_bnel = bits(insn, 0, 15);
   m_va_t new_pc_bnel;
   int res_bnel;

   /* compute the new pc */
   new_pc_bnel = (cpu->pc + 4) + sign_extend(offset_bnel << 2, 18);

   /* take the branch if gpr[rs] != gpr[rt] */
   res_bnel = (cpu->gpr[rs_bnel] != cpu->gpr[rt_bnel]);

   /* take the branch if the test result is true */
   if (res_bnel)
   {
      mips64_exec_bdslot(cpu);
      cpu->pc = new_pc_bnel;
   }
   else
      cpu->pc += 8;

   cpu_next_instruction(cpu,1,insn);


}
break_label:
{
   u_int code_break = bits(insn, 6, 25);
   mips64_exec_break(cpu, code_break);
   cpu_next_instruction(cpu,1,insn);
}
cache_label:
{
   int base_cache = bits(insn, 21, 25);
   int op_cache = bits(insn, 16, 20);
   int offset_cache = bits(insn, 0, 15);
   int cache_res;
   cache_res=mips64_exec_memop2(cpu, MIPS_MEMOP_CACHE, base_cache, offset_cache, op_cache, FALSE);
	cpu_next_instruction(cpu,cache_res,insn);
}

cfc0_label:
{
goto unknown_lable;
}
clz_label:
{
   int rs_clz = bits(insn, 21, 25);
   int rd_clz = bits(insn, 11, 15);
   int i_clz;
   m_uint32_t val_clz;
   val_clz = 32;
   for (i_clz = 31; i_clz >= 0; i_clz--)
   {
      if (cpu->gpr[rs_clz] & (1 << i_clz))
      {
         val_clz = 31 - i_clz;
         break;
      }
   }
   cpu->gpr[rd_clz] = val_clz;
   cpu_next_instruction(cpu,0,insn);

}
cop0_label:
{
   uint16_t special_func_cop0 = bits(insn, 21, 25);
   goto  *mips_cop0_labelcodes[special_func_cop0].label;
}

cop1_label:
{
#if SOFT_FPU
   mips64_exec_soft_fpu(cpu);
   cpu_next_instruction(cpu,1,insn);
#else
 goto unknown_lable;
#endif
}
cop1x_label:
{
#if SOFT_FPU
   mips64_exec_soft_fpu(cpu);
   cpu_next_instruction(cpu,1,insn);
#else
goto unknown_lable;
#endif
}

cop2_label:
{
goto unknown_lable;
}
dadd_label:
{
goto unknown_lable;
}

daddi_label:
{
goto unknown_lable;
}

daddiu_label:
{
goto unknown_lable;
}

daddu_label:
{
goto unknown_lable;
}

ddiv_label:
{
   goto unknown_lable;
}

ddivu_label:
{
   goto unknown_lable;
}

div_label:
{
   int rs_div = bits(insn, 21, 25);
   int rt_div = bits(insn, 16, 20);

   cpu->lo = (m_int32_t) cpu->gpr[rs_div] / (m_int32_t) cpu->gpr[rt_div];
   cpu->hi = (m_int32_t) cpu->gpr[rs_div] % (m_int32_t) cpu->gpr[rt_div];

   cpu->lo = sign_extend(cpu->lo, 32);
   cpu->hi = sign_extend(cpu->hi, 32);
   cpu_next_instruction(cpu,0,insn);

}
divu_label:
{
   int rs_divu = bits(insn, 21, 25);
   int rt_divu = bits(insn, 16, 20);

   if (cpu->gpr[rt_divu] == 0)
  		ASSERT(0,"0 can not be dived\n");

   cpu->lo = (m_uint32_t) cpu->gpr[rs_divu] / (m_uint32_t) cpu->gpr[rt_divu];
   cpu->hi = (m_uint32_t) cpu->gpr[rs_divu] % (m_uint32_t) cpu->gpr[rt_divu];

   cpu->lo = sign_extend(cpu->lo, 32);
   cpu->hi = sign_extend(cpu->hi, 32);
   cpu_next_instruction(cpu,0,insn);

}

dmfc0_label:
{
   goto unknown_lable;

}

dmtc0_label:
{
  goto unknown_lable;

}

dmult_label:
{
   goto unknown_lable;
}

dmultu_label:
{
goto unknown_lable;
}

dsll_label:
{
goto unknown_lable;
}

dsllv_label:
{
goto unknown_lable;
}

dsrlv_label:
{
   goto unknown_lable;
}

dsrav_label:
{
  goto unknown_lable;
}

dsub_label:
{
   goto unknown_lable;
}

dsubu_label:
{
   goto unknown_lable;
}

dsrl_label:
{
   goto unknown_lable;
}

dsra_label:
{
   goto unknown_lable;
}

dsll32_label:
{
   goto unknown_lable;
}

dsrl32_label:
{
   goto unknown_lable;
}

dsra32_label:
{
   goto unknown_lable;
}

eret_label:
{
  mips64_exec_eret(cpu);
  cpu_next_instruction(cpu,1,insn);
}

j_label:
{
   u_int instr_index_j = bits(insn, 0, 25);
   m_va_t new_pc_j;

   /* compute the new pc */
   new_pc_j = cpu->pc & ~((1 << 28) - 1);
   new_pc_j |= instr_index_j << 2;

   /* exec the instruction in the delay slot */
   int ins_res_j = mips64_exec_bdslot(cpu);
   if (likely(!ins_res_j))
      cpu->pc = new_pc_j;
    cpu_next_instruction(cpu,1,insn);
}

jal_label:
{
   u_int instr_index_jal = bits(insn, 0, 25);
   m_va_t new_pc_jal;

   /* compute the new pc */
   new_pc_jal = cpu->pc & ~((1 << 28) - 1);
   new_pc_jal |= instr_index_jal << 2;

   /* set the return address (instruction after the delay slot) */
   cpu->gpr[MIPS_GPR_RA] = cpu->pc + 8;

   int ins_res_jal = mips64_exec_bdslot(cpu);
   if (likely(!ins_res_jal))
      cpu->pc = new_pc_jal;

    cpu_next_instruction(cpu,1,insn);
}

jalr_label:
{
   int rs_jalr = bits(insn, 21, 25);
   int rd_jalr = bits(insn, 11, 15);
   m_va_t new_pc_jalr;

   /* set the return pc (instruction after the delay slot) in GPR[rd] */
   cpu->gpr[rd_jalr] = cpu->pc + 8;

   /* get the new pc */
   new_pc_jalr = cpu->gpr[rs_jalr];

   int ins_res_jalr = mips64_exec_bdslot(cpu);
   if (likely(!ins_res_jalr))
      cpu->pc = new_pc_jalr;
    cpu_next_instruction(cpu,1,insn);

}
jr_label:
{
   int rs_jr = bits(insn, 21, 25);
   m_va_t new_pc_jr;

   /* get the new pc */
   new_pc_jr = cpu->gpr[rs_jr];

   int ins_res_jr = mips64_exec_bdslot(cpu);
   if (likely(!ins_res_jr))
      cpu->pc = new_pc_jr;
    //cpu_log1(cpu,"","rs_jr %x new_pc_jr %x \n",rs_jr,new_pc_jr);
    cpu_next_instruction(cpu,1,insn);

}

lb_label:
{
   int base_lb = bits(insn, 21, 25);
   int rt_lb = bits(insn, 16, 20);
   int offset_lb = bits(insn, 0, 15);
   int lb_res;
   lb_res= (mips64_exec_memop2(cpu, MIPS_MEMOP_LB, base_lb, offset_lb, rt_lb, TRUE));
   cpu_next_instruction(cpu,lb_res,insn);
}

lbu_label:
{
   int base_lbu = bits(insn, 21, 25);
   int rt_lbu = bits(insn, 16, 20);
   int offset_lbu = bits(insn, 0, 15);
   int lbu_res;
   lbu_res= (mips64_exec_memop2(cpu, MIPS_MEMOP_LBU, base_lbu, offset_lbu, rt_lbu, TRUE));
   cpu_next_instruction(cpu,lbu_res,insn);

}
ld_label:
{
    goto unknown_lable;
}

ldc1_label:
{
    goto unknown_lable;
}

ldc2_label:
{
   goto unknown_lable;
}

ldl_label:
{
   goto unknown_lable;
}
ldr_label:
{
    goto unknown_lable;
}
lh_label:
{
   int base_lh = bits(insn, 21, 25);
   int rt_lh = bits(insn, 16, 20);
   int offset_lh = bits(insn, 0, 15);

   int lh_res;
   lh_res= (mips64_exec_memop2(cpu, MIPS_MEMOP_LH, base_lh, offset_lh, rt_lh, TRUE));
   cpu_next_instruction(cpu,lh_res,insn);

}

lhu_label:
{
   int base_lhu = bits(insn, 21, 25);
   int rt_lhu = bits(insn, 16, 20);
   int offset_lhu = bits(insn, 0, 15);

     int lhu_res;
   lhu_res= (mips64_exec_memop2(cpu, MIPS_MEMOP_LHU, base_lhu, offset_lhu, rt_lhu, TRUE));
   cpu_next_instruction(cpu,lhu_res,insn);
}

ll_label:
{
   int base_ll = bits(insn, 21, 25);
   int rt_ll = bits(insn, 16, 20);
   int offset_ll = bits(insn, 0, 15);
   
   int ll_res;
   ll_res=(mips64_exec_memop2(cpu, MIPS_MEMOP_LL, base_ll, offset_ll, rt_ll, TRUE));
   cpu_next_instruction(cpu,ll_res,insn);


}
lld_label:
{
    goto unknown_lable;
}

lui_label:
{
   int rt_lui = bits(insn, 16, 20);
   int imm_lui = bits(insn, 0, 15);

   cpu->gpr[rt_lui] = sign_extend(imm_lui, 16) << 16;
   cpu_next_instruction(cpu,0,insn);
}

 lw_label:
{
   int base_lw = bits(insn, 21, 25);
   int rt_lw = bits(insn, 16, 20);
   int offset_lw = bits(insn, 0, 15);

   int lw_res;
   lw_res=(mips64_exec_memop2(cpu, MIPS_MEMOP_LW, base_lw, offset_lw, rt_lw, TRUE));
   cpu_next_instruction(cpu,lw_res,insn);

}
 lwc1_label:
{
#if SOFT_FPU
   mips64_exec_soft_fpu(cpu);
   cpu_next_instruction(cpu,1,insn);
#else
    goto unknown_lable;
#endif

}

lwc2_label:
{
    goto unknown_lable;
 }

lwl_label:
{
   int base_lwl = bits(insn, 21, 25);
   int rt_lwl = bits(insn, 16, 20);
   int offset_lwl = bits(insn, 0, 15);

   int lwl_res;
   lwl_res= (mips64_exec_memop2(cpu, MIPS_MEMOP_LWL, base_lwl, offset_lwl, rt_lwl, TRUE));
   cpu_next_instruction(cpu,lwl_res,insn);

}

lwr_label:
{
   int base_lwr = bits(insn, 21, 25);
   int rt_lwr = bits(insn, 16, 20);
   int offset_lwr = bits(insn, 0, 15);


   int lwr_res;
   lwr_res=  (mips64_exec_memop2(cpu, MIPS_MEMOP_LWR, base_lwr, offset_lwr, rt_lwr, TRUE));
   cpu_next_instruction(cpu,lwr_res,insn);


}

lwu_label:
{
   int base_lwu = bits(insn, 21, 25);
   int rt_lwu = bits(insn, 16, 20);
   int offset_lwu = bits(insn, 0, 15);

   int lwu_res;
   lwu_res= (mips64_exec_memop2(cpu, MIPS_MEMOP_LWU, base_lwu, offset_lwu, rt_lwu, TRUE));
   cpu_next_instruction(cpu,lwu_res,insn);

}

mad_label:
{
   int index_mad = bits(insn, 0, 5);
   goto  *mips_mad_labelcodes[index_mad].label;
   
}

madd_label:
{
   int rs_madd = bits(insn, 21, 25);
   int rt_madd = bits(insn, 16, 20);
   m_int64_t val_madd, temp_madd;

   val_madd = (m_int32_t) (m_int32_t) cpu->gpr[rs_madd];
   val_madd *= (m_int32_t) (m_int32_t) cpu->gpr[rt_madd];

   temp_madd = cpu->hi;
   temp_madd = temp_madd << 32;
   temp_madd += cpu->lo;
   val_madd += temp_madd;

   cpu->lo = sign_extend(val_madd, 32);
   cpu->hi = sign_extend(val_madd >> 32, 32);
   cpu_next_instruction(cpu,0,insn);

}

maddu_label:
{
   int rs_maddu = bits(insn, 21, 25);
   int rt_maddu = bits(insn, 16, 20);
   m_int64_t val_maddu, temp_maddu;

   val_maddu = (m_uint32_t) (m_uint32_t) cpu->gpr[rs_maddu];
   val_maddu *= (m_uint32_t) (m_uint32_t) cpu->gpr[rt_maddu];

   temp_maddu = cpu->hi;
   temp_maddu = temp_maddu << 32;
   temp_maddu += cpu->lo;
   val_maddu += temp_maddu;


   cpu->lo = sign_extend(val_maddu, 32);
   cpu->hi = sign_extend(val_maddu >> 32, 32);
    cpu_next_instruction(cpu,0,insn);

}

mfc0_label:
{
   int rt_mfc0 = bits(insn, 16, 20);
   int rd_mfc0 = bits(insn, 11, 15);
   int sel_mfc0 = bits(insn, 0, 2);

   mips64_cp0_exec_mfc0(cpu, rt_mfc0, rd_mfc0, sel_mfc0);
    cpu_next_instruction(cpu,0,insn);

}

 mfhi_label:
{
   int rd_label = bits(insn, 11, 15);

   if (rd_label)
      cpu->gpr[rd_label] = cpu->hi;
   cpu_next_instruction(cpu,0,insn);

}

mflo_label:
{
   int rd_mflo = bits(insn, 11, 15);

   if (rd_mflo)
      cpu->gpr[rd_mflo] = cpu->lo;
   cpu_next_instruction(cpu,0,insn);

}

movc_label:
{
   goto unknown_lable;
}

movz_label:
{
   int rs_movz = bits(insn, 21, 25);
   int rd_movz = bits(insn, 11, 15);
   int rt_movz = bits(insn, 16, 20);


   if ((cpu->gpr[rt_movz]) == 0)
      cpu->gpr[rd_movz] = sign_extend(cpu->gpr[rs_movz], 32);
    cpu_next_instruction(cpu,0,insn);

}

movn_label:
{
   int rs_movn = bits(insn, 21, 25);
   int rd_movn = bits(insn, 11, 15);
   int rt_movn = bits(insn, 16, 20);

   if ((cpu->gpr[rt_movn]) != 0)
      cpu->gpr[rd_movn] = sign_extend(cpu->gpr[rs_movn], 32);
    cpu_next_instruction(cpu,0,insn);

}

msub_label:
{
   int rs_msub = bits(insn, 21, 25);
   int rt_msub = bits(insn, 16, 20);
   m_int64_t val_msub, temp_msub;

   val_msub = (m_int32_t) (m_int32_t) cpu->gpr[rs_msub];
   val_msub *= (m_int32_t) (m_int32_t) cpu->gpr[rt_msub];

   temp_msub = cpu->hi;
   temp_msub = temp_msub << 32;
   temp_msub += cpu->lo;

   temp_msub -= val_msub;
   //val += temp;

   cpu->lo = sign_extend(temp_msub, 32);
   cpu->hi = sign_extend(temp_msub >> 32, 32);
    cpu_next_instruction(cpu,0,insn);

}

msubu_label:
{
   int rs_msubu = bits(insn, 21, 25);
   int rt_msubu = bits(insn, 16, 20);
   m_int64_t val_msubu, temp_msubu;

   val_msubu = (m_uint32_t) (m_uint32_t) cpu->gpr[rs_msubu];
   val_msubu *= (m_uint32_t) (m_uint32_t) cpu->gpr[rt_msubu];

   temp_msubu = cpu->hi;
   temp_msubu = temp_msubu << 32;
   temp_msubu += cpu->lo;

   temp_msubu -= val_msubu;
   //val += temp;

   cpu->lo = sign_extend(temp_msubu, 32);
   cpu->hi = sign_extend(temp_msubu >> 32, 32);
   cpu_next_instruction(cpu,0,insn);

}

mtc0_label:
{
   int rt_mtc0 = bits(insn, 16, 20);
   int rd_mtc0 = bits(insn, 11, 15);
   int sel_mtc0 = bits(insn, 0, 2);
   mips64_cp0_exec_mtc0(cpu, rt_mtc0, rd_mtc0, sel_mtc0);
    cpu_next_instruction(cpu,0,insn);

}

mthi_label:
{
   int rs_mthi = bits(insn, 21, 25);

   cpu->hi = cpu->gpr[rs_mthi];
    cpu_next_instruction(cpu,0,insn);
}

mtlo_label:
{
   int rs_mtlo = bits(insn, 21, 25);

   cpu->lo = cpu->gpr[rs_mtlo];
    cpu_next_instruction(cpu,0,insn);

}

mul_label:
{
   int rs_mul = bits(insn, 21, 25);
   int rt_mul = bits(insn, 16, 20);
   int rd_mul = bits(insn, 11, 15);
   m_int32_t val_mul;

   /* note: after this instruction, HI/LO regs are undefined */
   val_mul = (m_int32_t) cpu->gpr[rs_mul] * (m_int32_t) cpu->gpr[rt_mul];
   cpu->gpr[rd_mul] = sign_extend(val_mul, 32);
    cpu_next_instruction(cpu,0,insn);

}

mult_label:
{
   int rs_mult = bits(insn, 21, 25);
   int rt_mult = bits(insn, 16, 20);
   m_int64_t val_mult;

   val_mult = (m_int64_t) (m_int32_t) cpu->gpr[rs_mult];
   val_mult *= (m_int64_t) (m_int32_t) cpu->gpr[rt_mult];

   cpu->lo = sign_extend(val_mult, 32);
   cpu->hi = sign_extend(val_mult >> 32, 32);
    cpu_next_instruction(cpu,0,insn);

}
multu_label:
{
   int rs_multu = bits(insn, 21, 25);
   int rt_multu = bits(insn, 16, 20);
   m_int64_t val_multu;               //must be 64 bit. not m_reg_t !!!

   val_multu = (m_reg_t) (m_uint32_t) cpu->gpr[rs_multu];
   val_multu *= (m_reg_t) (m_uint32_t) cpu->gpr[rt_multu];
   cpu->lo = sign_extend(val_multu, 32);
   cpu->hi = sign_extend(val_multu >> 32, 32);
    cpu_next_instruction(cpu,0,insn);

}
nor_label:
{
   int rs_nor = bits(insn, 21, 25);
   int rt_nor = bits(insn, 16, 20);
   int rd_nor = bits(insn, 11, 15);

   cpu->gpr[rd_nor] = ~(cpu->gpr[rs_nor] | cpu->gpr[rt_nor]);
    cpu_next_instruction(cpu,0,insn);

}

or_label:
{
   int rs_or = bits(insn, 21, 25);
   int rt_or = bits(insn, 16, 20);
   int rd_or = bits(insn, 11, 15);

   cpu->gpr[rd_or] = cpu->gpr[rs_or] | cpu->gpr[rt_or];
    cpu_next_instruction(cpu,0,insn);

}

ori_label:
{
   int rs_ori = bits(insn, 21, 25);
   int rt_ori = bits(insn, 16, 20);
   int imm_ori = bits(insn, 0, 15);

   cpu->gpr[rt_ori] = cpu->gpr[rs_ori] | imm_ori;
   cpu_next_instruction(cpu,0,insn);

}

pref_label:
{
    cpu_next_instruction(cpu,0,insn);
}

sb_label:
{
   int base_sb = bits(insn, 21, 25);
   int rt_sb = bits(insn, 16, 20);
   int offset_sb = bits(insn, 0, 15);
   int sb_res;
   sb_res= (mips64_exec_memop2(cpu, MIPS_MEMOP_SB, base_sb, offset_sb, rt_sb, FALSE));
   cpu_next_instruction(cpu,sb_res,insn);
}

sc_label:
{
   int base_sc = bits(insn, 21, 25);
   int rt_sc = bits(insn, 16, 20);
   int offset_sc = bits(insn, 0, 15);

   int sc_res;
   sc_res= (mips64_exec_memop2(cpu, MIPS_MEMOP_SC, base_sc, offset_sc, rt_sc, TRUE));
   cpu_next_instruction(cpu,sc_res,insn);

}

scd_label:
{
    goto unknown_lable;
}

sd_label:
{
    goto unknown_lable;
}

sdc1_label:
{
#if SOFT_FPU
   mips64_exec_soft_fpu(cpu);
   cpu_next_instruction(cpu,1,insn);
#else
    goto unknown_lable;
#endif

}

sdc2_label:
{
    goto unknown_lable;
}


sdl_label:
{
    goto unknown_lable;
}

sdr_label:
{
    goto unknown_lable;
}

sh_label:
{
   int base_sh = bits(insn, 21, 25);
   int rt_sh = bits(insn, 16, 20);
   int offset_sh = bits(insn, 0, 15);


    int sh_res;
   sh_res= (mips64_exec_memop2(cpu, MIPS_MEMOP_SH, base_sh, offset_sh, rt_sh, FALSE));
   cpu_next_instruction(cpu,sh_res,insn);


}

sll_label:
{
   int rt_sll = bits(insn, 16, 20);
   int rd_sll = bits(insn, 11, 15);
   int sa_sll = bits(insn, 6, 10);
   m_uint32_t res_sll;

   res_sll = (m_uint32_t) cpu->gpr[rt_sll] << sa_sll;
   cpu->gpr[rd_sll] = sign_extend(res_sll, 32);
   cpu_next_instruction(cpu,0,insn);

}

sllv_label:
{
   int rs_sllv = bits(insn, 21, 25);
   int rt_sllv = bits(insn, 16, 20);
   int rd_sllv = bits(insn, 11, 15);
   m_uint32_t res_sllv;

   res_sllv = (m_uint32_t) cpu->gpr[rt_sllv] << (cpu->gpr[rs_sllv] & 0x1f);
   cpu->gpr[rd_sllv] = sign_extend(res_sllv, 32);
  cpu_next_instruction(cpu,0,insn);

}

slt_label:
{
   int rs_slt = bits(insn, 21, 25);
   int rt_slt = bits(insn, 16, 20);
   int rd_slt = bits(insn, 11, 15);

   if ((m_ireg_t) cpu->gpr[rs_slt] < (m_ireg_t) cpu->gpr[rt_slt])
      cpu->gpr[rd_slt] = 1;
   else
      cpu->gpr[rd_slt] = 0;

   cpu_next_instruction(cpu,0,insn);
}

slti_label:
{
   int rs_slti = bits(insn, 21, 25);
   int rt_slti = bits(insn, 16, 20);
   int imm_slti = bits(insn, 0, 15);
   m_ireg_t val_slti = sign_extend(imm_slti, 16);

   if ((m_ireg_t) cpu->gpr[rs_slti] < val_slti)
      cpu->gpr[rt_slti] = 1;
   else
      cpu->gpr[rt_slti] = 0;

   cpu_next_instruction(cpu,0,insn);
}


sltiu_label:
{
   int rs_sltiu = bits(insn, 21, 25);
   int rt_sltiu = bits(insn, 16, 20);
   int imm_sltiu = bits(insn, 0, 15);
   m_reg_t val_sltiu = sign_extend(imm_sltiu, 16);

   if (cpu->gpr[rs_sltiu] < val_sltiu)
      cpu->gpr[rt_sltiu] = 1;
   else
      cpu->gpr[rt_sltiu] = 0;

  cpu_next_instruction(cpu,0,insn);
}

sltu_label:
{
   int rs_sltu = bits(insn, 21, 25);
   int rt_sltu = bits(insn, 16, 20);
   int rd_sltu = bits(insn, 11, 15);


   if (cpu->gpr[rs_sltu] < cpu->gpr[rt_sltu])
      cpu->gpr[rd_sltu] = 1;
   else
      cpu->gpr[rd_sltu] = 0;

  cpu_next_instruction(cpu,0,insn);

}

spec_label:
{
   uint16_t special_func = bits(insn, 0, 5);
   goto  *mips_spec_labelcodes[special_func].label;
}

sra_label:
{
   int rt_sra = bits(insn, 16, 20);
   int rd_sra = bits(insn, 11, 15);
   int sa_sra = bits(insn, 6, 10);
   m_int32_t res_sra;

   res_sra = (m_int32_t) cpu->gpr[rt_sra] >> sa_sra;
   cpu->gpr[rd_sra] = sign_extend(res_sra, 32);
  cpu_next_instruction(cpu,0,insn);

}

srav_label:
{
   int rs_srav = bits(insn, 21, 25);
   int rt_srav = bits(insn, 16, 20);
   int rd_srav = bits(insn, 11, 15);
   m_int32_t res_srav;

   res_srav= (m_int32_t) cpu->gpr[rt_srav] >> (cpu->gpr[rs_srav] & 0x1f);
   cpu->gpr[rd_srav] = sign_extend(res_srav, 32);
   cpu_next_instruction(cpu,0,insn);

}

srl_label:
{
   int rt_srl = bits(insn, 16, 20);
   int rd_srl = bits(insn, 11, 15);
   int sa_srl = bits(insn, 6, 10);
   m_uint32_t res_srl;

   res_srl = (m_uint32_t) cpu->gpr[rt_srl] >> sa_srl;
   cpu->gpr[rd_srl] = sign_extend(res_srl, 32);
  cpu_next_instruction(cpu,0,insn);

}

srlv_label:
{
   int rs_srlv = bits(insn, 21, 25);
   int rt_srlv = bits(insn, 16, 20);
   int rd_srlv = bits(insn, 11, 15);
   m_uint32_t res_srlv;

   res_srlv = (m_uint32_t) cpu->gpr[rt_srlv] >> (cpu->gpr[rs_srlv] & 0x1f);
   cpu->gpr[rd_srlv] = sign_extend(res_srlv, 32);
   cpu_next_instruction(cpu,0,insn);

}

sub_label:
{
   int rs_sub = bits(insn, 21, 25);
   int rt_sub = bits(insn, 16, 20);
   int rd_sub = bits(insn, 11, 15);
   m_uint32_t res_sub;

   /* TODO: Exception handling */
   res_sub = (m_uint32_t) cpu->gpr[rs_sub] - (m_uint32_t) cpu->gpr[rt_sub];
   cpu->gpr[rd_sub] = sign_extend(res_sub, 32);
   cpu_next_instruction(cpu,0,insn);

}

subu_label:
{
   int rs_subu = bits(insn, 21, 25);
   int rt_subu = bits(insn, 16, 20);
   int rd_subu = bits(insn, 11, 15);
   m_uint32_t res_subu;
   res_subu = (m_uint32_t) cpu->gpr[rs_subu] - (m_uint32_t) cpu->gpr[rt_subu];
   cpu->gpr[rd_subu] = sign_extend(res_subu, 32);

   cpu_next_instruction(cpu,0,insn);

}

sw_label:
{
   int base_sw = bits(insn, 21, 25);
   int rt_sw = bits(insn, 16, 20);
   int offset_sw = bits(insn, 0, 15);

   int sw_res;
   m_pa_t exec_guest_page_sw;

   sw_res= (mips64_exec_memop2(cpu, MIPS_MEMOP_SW, base_sw, offset_sw, rt_sw, FALSE));
   	
   cpu_next_instruction(cpu,sw_res,insn);

}

swc1_label:
{
#if SOFT_FPU
   mips64_exec_soft_fpu(cpu);
  cpu_next_instruction(cpu,1,insn);
#else
    goto unknown_lable;
#endif

}

swc2_label:
{
    goto unknown_lable;
}

swl_label:
{
   int base_swl = bits(insn, 21, 25);
   int rt_swl = bits(insn, 16, 20);
   int offset_swl = bits(insn, 0, 15);



    int swl_res;

   swl_res= (mips64_exec_memop2(cpu, MIPS_MEMOP_SWL, base_swl, offset_swl, rt_swl, FALSE));
   cpu_next_instruction(cpu,swl_res,insn);


}

swr_label:
{
   int base_swr = bits(insn, 21, 25);
   int rt_swr = bits(insn, 16, 20);
   int offset_swr = bits(insn, 0, 15);



    int swr_res;

   swr_res=  (mips64_exec_memop2(cpu, MIPS_MEMOP_SWR, base_swr, offset_swr, rt_swr, FALSE));
   cpu_next_instruction(cpu,swr_res,insn);

}

sync_label:
{
    cpu_next_instruction(cpu,0,insn);
}

syscall_label:
{
   mips64_exec_syscall(cpu);
  cpu_next_instruction(cpu,1,insn);
}

teq_label:
{
   int rs_teq = bits(insn, 21, 25);
   int rt_teq = bits(insn, 16, 20);

   if (unlikely(cpu->gpr[rs_teq] == cpu->gpr[rt_teq]))
   {
      mips64_trigger_trap_exception(cpu);
      cpu_next_instruction(cpu,1,insn);
   }

   cpu_next_instruction(cpu,0,insn);

}

teqi_label:
{
   int rs_teqi = bits(insn, 21, 25);
   int imm_teqi = bits(insn, 0, 15);
   m_reg_t val_teqi = sign_extend(imm_teqi, 16);

   if (unlikely(cpu->gpr[rs_teqi] == val_teqi))
   {
      mips64_trigger_trap_exception(cpu);
     cpu_next_instruction(cpu,1,insn);
   }

   cpu_next_instruction(cpu,0,insn);

}

tlb_label:
{
   uint16_t func_tlb = bits(insn, 0, 5);
   goto  *mips_tlb_labelcodes[func_tlb].label;
}

tlbp_label:
{
   mips64_cp0_exec_tlbp(cpu);
   cpu_next_instruction(cpu,0,insn);
}

tlbr_label:
{
   mips64_cp0_exec_tlbr(cpu);
   cpu_next_instruction(cpu,0,insn);
}

tlbwi_label:
{
   mips64_cp0_exec_tlbwi(cpu);
   cpu_next_instruction(cpu,0,insn);
}

tlbwr_label:
{
   mips64_cp0_exec_tlbwr(cpu);
   cpu_next_instruction(cpu,0,insn);
}



tge_label:
{
    goto unknown_lable;
}

tgei_label:
{
    goto unknown_lable;
}

tgeiu_label:
{
    goto unknown_lable;
}

tgeu_label:
{
    goto unknown_lable;
}

tlt_label:
{
   goto unknown_lable;
}

tlti_label:
{
    goto unknown_lable;
}

tltiu_label:
{
    goto unknown_lable;
}


tltu_label:
{
   goto unknown_lable;
}

tne_label:
{
   int rs_tne = bits(insn, 21, 25);
   int rt_tne = bits(insn, 16, 20);

   if ((m_ireg_t) cpu->gpr[rs_tne] != (m_ireg_t) cpu->gpr[rt_tne])
   {
      /*take a trap */
      mips64_trigger_trap_exception(cpu);
      cpu_next_instruction(cpu,1,insn);
   }
   else
      cpu_next_instruction(cpu,0,insn);

}

tnei_label:
{
    goto unknown_lable;
}

wait_label:
{
   cpu_next_instruction(cpu,0,insn);
}

xor_label:
{
   int rs_xor = bits(insn, 21, 25);
   int rt_xor = bits(insn, 16, 20);
   int rd_xor = bits(insn, 11, 15);

   cpu->gpr[rd_xor] = cpu->gpr[rs_xor] ^ cpu->gpr[rt_xor];
  cpu_next_instruction(cpu,0,insn);
}

xori_label:
{
   int rs_xori = bits(insn, 21, 25);
   int rt_xori = bits(insn, 16, 20);
   int imm_xori = bits(insn, 0, 15);

   cpu->gpr[rt_xori] = cpu->gpr[rs_xori] ^ imm_xori;
   cpu_next_instruction(cpu,0,insn);

}


undef_label:
{
    goto unknown_lable;
}

unknownBcondlabel:
{
    goto unknown_lable;

}

unknowncop0_label:
{
    goto unknown_lable;
}

unknownmad_label:
{
    goto unknown_lable;
}

unknownSpeclabel:
{
    goto unknown_lable;
}

unknowntlb_label:
{
    goto unknown_lable;
}


return NULL;
 
}




#include "mips64_codetable.h"


#endif



