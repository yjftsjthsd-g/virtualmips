
#ifndef __MIPS64_JIT_H__
#define __MIPS64_JIT_H__

#include "system.h"
#include "rbtree.h"
#include "utils.h"
#include "sbox.h"
#include "mips64.h"


/* Size of executable page area (in Mb) */
#ifndef __CYGWIN__
#define MIPS_EXEC_AREA_SIZE  64
#else
#define MIPS_EXEC_AREA_SIZE  16
#endif

/* Buffer size for JIT code generation */
#define MIPS_JIT_BUFSIZE     32768

/* Maximum number of X86 chunks */
#define MIPS_JIT_MAX_CHUNKS  32

/* Size of hash for PC lookup */
#define MIPS_JIT_PC_HASH_BITS   16
#define MIPS_JIT_PC_HASH_MASK   ((1 << MIPS_JIT_PC_HASH_BITS) - 1)
#define MIPS_JIT_PC_HASH_SIZE   (1 << MIPS_JIT_PC_HASH_BITS)

/* Instruction jump patch */
struct mips64_insn_patch {
   u_char *jit_insn;
   m_uint64_t mips_pc;
};

/* Instruction patch table */
#define MIPS64_INSN_PATCH_TABLE_SIZE  32

struct mips64_jit_patch_table {
   struct mips64_insn_patch patches[MIPS64_INSN_PATCH_TABLE_SIZE];
   u_int cur_patch;
   struct mips64_jit_patch_table *next;
};

/* Host executable page */
struct insn_exec_page {
   u_char *ptr;
   insn_exec_page_t *next;
};

/* MIPS64 translated code block */
struct mips64_jit_tcb {
	/*start pc in tcb*/
   m_uint64_t start_pc;   
	m_uint64_t acc_count;
	/*guest pc to host pc mapping table*/
   u_char **jit_insn_ptr;
	/*guest code of this tcb*/
   mips_insn_t *mips_code;
   u_int mips_trans_pos;
   u_int jit_chunk_pos;
   /*used in translating*/
   u_char *jit_ptr;
   insn_exec_page_t *jit_buffer;
   insn_exec_page_t *jit_chunks[MIPS_JIT_MAX_CHUNKS];
   struct mips64_jit_patch_table *patch_table;
   mips64_jit_tcb_t *prev,*next;
};


/* Get the JIT instruction pointer in a translated block */
static forced_inline 
u_char *mips64_jit_tcb_get_host_ptr(mips64_jit_tcb_t *b,m_va_t vaddr)
{
   m_uint32_t offset;

   offset = ((m_uint32_t)vaddr & MIPS_MIN_PAGE_IMASK) >> 2;
   return(b->jit_insn_ptr[offset]);
}

/* Check if the specified address belongs to the specified block */
static forced_inline 
int mips64_jit_tcb_local_addr(mips64_jit_tcb_t *block,m_va_t vaddr,
                              u_char **jit_addr)
{
   if ((vaddr & MIPS_MIN_PAGE_MASK) == block->start_pc) {
      *jit_addr = mips64_jit_tcb_get_host_ptr(block,vaddr);
      return(1);
   }

   return(0);
}

/* Check if PC register matches the compiled block virtual address */
static forced_inline 
int mips64_jit_tcb_match(cpu_mips_t *cpu,mips64_jit_tcb_t *block)
{
   m_va_t vpage;

   vpage = cpu->pc & ~(m_uint64_t)MIPS_MIN_PAGE_IMASK;
   return(block->start_pc == vpage);
}

/* Compute the hash index for the specified PC value */
static forced_inline m_uint32_t mips64_jit_get_pc_hash(m_va_t pc)
{
   m_uint32_t page_hash;

   page_hash = sbox_u32(pc >> MIPS_MIN_PAGE_SHIFT);
   return((page_hash ^ (page_hash >> 12)) & MIPS_JIT_PC_HASH_MASK);
}



#endif


