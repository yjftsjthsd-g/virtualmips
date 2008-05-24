#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>
#include <fcntl.h>
#include <assert.h>
#include <setjmp.h>

#include "cpu.h"
#include "vm.h"
#include "mips64_exec.h"
#include "mips64_memory.h"
#include "mips64.h"
#include "mips64_cp0.h"
#include "debug.h"
#include "vp_timer.h"
#include "mips64_hostalarm.h"
#include "x86_trans.h"


extern struct mips64_jit_desc mips_jit[];
extern cpu_mips_t *current_cpu;
jmp_buf run_jmp;   
static void forced_inline mips64_main_loop_wait(cpu_mips_t * cpu, int timeout)
{
   vp_run_timers(&active_timers[VP_TIMER_REALTIME], vp_get_clock(rt_clock));
}
/* Initialize the JIT structure */
int mips64_jit_init(cpu_mips_t *cpu)
{
   insn_exec_page_t *cp;
   u_char *cp_addr;
   u_int area_size;
   size_t len;
   int i;

   /* Physical mapping for executable pages */
   len = MIPS_JIT_PC_HASH_SIZE * sizeof(void *);
   cpu->exec_blk_map = m_memalign(4096,len);
   memset(cpu->exec_blk_map,0,len);

   /* Get area size */
   area_size = MIPS_EXEC_AREA_SIZE;

   /* Create executable page area */
   cpu->exec_page_area_size = area_size * 1048576;
   cpu->exec_page_area = mmap(NULL,cpu->exec_page_area_size,
                              PROT_EXEC|PROT_READ|PROT_WRITE,
                              MAP_SHARED|MAP_ANONYMOUS,-1,(off_t)0);

   if (!cpu->exec_page_area) {
      fprintf(stderr,
              "mips64_jit_init: unable to create exec area (size %lu)\n",
              (u_long)cpu->exec_page_area_size);
      return(-1);
   }

   /* Carve the executable page area */
   cpu->exec_page_count = cpu->exec_page_area_size / MIPS_JIT_BUFSIZE;

   cpu->exec_page_array = calloc(cpu->exec_page_count,
                                 sizeof(insn_exec_page_t));
   
   if (!cpu->exec_page_array) {
      fprintf(stderr,"mips64_jit_init: unable to create exec page array\n");
      return(-1);
   }

   for(i=0,cp_addr=cpu->exec_page_area;i<cpu->exec_page_count;i++) {
      cp = &cpu->exec_page_array[i];

      cp->ptr = cp_addr;
      cp_addr += MIPS_JIT_BUFSIZE;

      cp->next = cpu->exec_page_free_list;
      cpu->exec_page_free_list = cp;
   }

   printf("CPU%u: carved JIT exec zone of %lu Mb into %lu pages of %u Kb.\n",
          cpu->id,
          (u_long)(cpu->exec_page_area_size / 1048576),
          (u_long)cpu->exec_page_count,MIPS_JIT_BUFSIZE / 1024);
   return(0);
}

/* Flush the JIT */
u_int mips64_jit_flush(cpu_mips_t *cpu,u_int threshold)
{
   mips64_jit_tcb_t *p,*next;
   m_uint32_t pc_hash;
   u_int count = 0;

   if (!threshold)
      threshold = (u_int)(-1);  /* UINT_MAX not defined everywhere */

   for(p=cpu->tcb_list;p;p=next) {
      next = p->next;

      if (p->acc_count <= threshold) {
         pc_hash = mips64_jit_get_pc_hash(p->start_pc);
         cpu->exec_blk_map[pc_hash] = NULL;
         mips64_jit_tcb_free(cpu,p,TRUE);
         count++;
      }
   }

   cpu->compiled_pages -= count;
   return(count);
}

/* Shutdown the JIT */
void mips64_jit_shutdown(cpu_mips_t *cpu)
{   
   mips64_jit_tcb_t *p,*next;

   /* Flush the JIT */
   mips64_jit_flush(cpu,0);

   /* Free the instruction blocks */
   for(p=cpu->tcb_free_list;p;p=next) {
      next = p->next;
      free(p);
   }

   /* Unmap the executable page area */
   if (cpu->exec_page_area)
      munmap(cpu->exec_page_area,cpu->exec_page_area_size);

   /* Free the exec page array */
   free(cpu->exec_page_array);

   /* Free physical mapping for executable pages */
   free(cpu->exec_blk_map);   
}

/* Allocate an exec page */
static inline insn_exec_page_t *exec_page_alloc(cpu_mips_t *cpu)
{
   insn_exec_page_t *p;
   u_int count;

   /* If the free list is empty, flush JIT */
   if (unlikely(!cpu->exec_page_free_list)) 
   {
      if (cpu->jit_flush_method) {
         cpu_log(cpu,
                 "JIT","flushing data structures (compiled pages=%u)\n",
                 cpu->compiled_pages);
         mips64_jit_flush(cpu,0);
      } else {
         count = mips64_jit_flush(cpu,100);
         cpu_log(cpu,"JIT","partial JIT flush (count=%u)\n",count);

         if (!cpu->exec_page_free_list)
            mips64_jit_flush(cpu,0);
      }
      
      /* Use both methods alternatively */
      cpu->jit_flush_method = 1 - cpu->jit_flush_method;
   }

   if (unlikely(!(p = cpu->exec_page_free_list)))
      return NULL;
   
   cpu->exec_page_free_list = p->next;
   cpu->exec_page_alloc++;
   return p;
}

/* Free an exec page and returns it to the pool */
static inline void exec_page_free(cpu_mips_t *cpu,insn_exec_page_t *p)
{
   if (p) {
      p->next = cpu->exec_page_free_list;
      cpu->exec_page_free_list = p;
      cpu->exec_page_alloc--;
   }
}




/* Fetch a MIPS instruction */
static forced_inline mips_insn_t insn_fetch(mips64_jit_tcb_t *b)
{
   return(vmtoh32(b->mips_code[b->mips_trans_pos]));
}

#if 0

/* Check if an instruction is in a delay slot or not */
int mips64_jit_is_delay_slot(mips64_jit_tcb_t *b,m_va_t pc)
{   
   struct mips64_insn_tag *tag;
   m_uint32_t offset,insn;

   offset = (pc - b->start_pc) >> 2;

   if (!offset)
      return(FALSE);

   /* Fetch the previous instruction to determine if it is a jump */
   insn = vmtoh32(b->mips_code[offset-1]);
   tag = insn_tag_find(insn);
   assert(tag != NULL);
   return(!tag->delay_slot);
}

#endif
int tttt;
void fastcall jit_debug(cpu_mips_t *cpu,mips64_jit_tcb_t *block)
{
  	//if  ((cpu->jit_pc==0x802f9660)||(cpu->jit_pc==0x802f965c)||(cpu->jit_pc==0x802f9664))
  	if (cpu->jit_pc==0x802e8040)
  		tttt=1;
  	if (tttt)
  		{
  			cpu_log2(cpu,"","pc %x \n",cpu->jit_pc);
  			if (cpu->jit_pc==0x80114444)
  				cpu_log2(cpu,"","a0 %x a1 %x \n",cpu->gpr[4],cpu->gpr[5]);
  			if ((cpu->jit_pc>=0x801141cc)&&(cpu->jit_pc<=0x801141e0))
  				cpu_log2(cpu,"","vo %x\n",cpu->gpr[2]);
  		}
  		
	 if ((cpu->vm->mipsy_debug_mode)&&((cpu_hit_breakpoint(cpu->vm,cpu->jit_pc) == SUCCESS)||(cpu->vm->gdb_interact_sock==-1)||( cpu->vm->mipsy_break_nexti ==MIPS_BREAKANYCPU)))
	 	{
	 		if (mips_debug(cpu->vm,1))
	 		  {
	 		      
	 		  }
	 	}
}

/* Fetch a MIPS instruction and emit corresponding translated code */
int mips64_jit_fetch_and_emit(cpu_mips_t *cpu,
                                                  mips64_jit_tcb_t *block,
                                                  int delay_slot)
{
   mips_insn_t code;
register uint op;
   code = insn_fetch(block);
   //cpu_log1(cpu,"","block->mips_trans_pos %x\n",block->mips_trans_pos);

   /* Branch-delay slot is in another page: slow exec */
 /*  if ((block->mips_trans_pos == (MIPS_INSN_PER_PAGE-1)) ) {
      block->jit_insn_ptr[block->mips_trans_pos] = block->jit_ptr;

      mips64_set_pc(block,block->start_pc + (block->mips_trans_pos << 2));
      mips64_emit_single_step(block,code); 
      mips64_jit_tcb_push_epilog(block);
      block->mips_trans_pos++;
      return (0) ;
   }
*/
 //  if (delay_slot && !tag->delay_slot) {
  //    mips64_emit_invalid_delay_slot(block);
  //    return NULL;
  // }


   if (!delay_slot)
      block->jit_insn_ptr[block->mips_trans_pos] = block->jit_ptr;


   m_uint32_t  temp_pc;
//save jit pc
//if (delay_slot!=2)
temp_pc=block->start_pc + ((block->mips_trans_pos) << 2);
//else
//	temp_pc=block->start_pc + ((block->mips_trans_pos-1) << 2);
if (block->start_pc==0x80119000)
   	{
   		cpu_log2(cpu,"","ptr1 %x mips_trans_pos %x \n",block->jit_ptr,block->mips_trans_pos);
   	}

 x86_mov_membase_imm(block->jit_ptr,X86_EDI,OFFSET(cpu_mips_t,jit_pc),temp_pc,4);

x86_mov_reg_reg(block->jit_ptr,X86_EAX,X86_EDI,4);
x86_mov_reg_imm(block->jit_ptr,X86_EDX,block);
mips64_emit_basic_c_call(block,jit_debug);
   op = MAJOR_OP(code);
  if (block->start_pc==0x80119000)
   	{
   		cpu_log2(cpu,"","jitpc temp_pc %x name %s delay_slot %x insn %x \n",temp_pc,mips_jit[op].opname,delay_slot,code);
   	}
   if (delay_slot!=2 )
      block->mips_trans_pos++;
   


   if (!delay_slot) {
      /* Check for IRQs + Increment count register before jumps */
     // if (!tag->delay_slot) {
     	  mips64_check_cpu_pausing(block);
         mips64_check_pending_irq(block);
      //}
   }

   mips_jit[op].emit_func(cpu,block, code);
   /*yajin*/
   //if (delay_slot==2)
   	//	block->mips_trans_pos++;//we have added if delay slot is not 2
   if (block->start_pc==0x80119000)
   	{
   		cpu_log2(cpu,"","ptr2 %x\n",block->jit_ptr);
   	}

	return (0);
}

/* Add end of JIT block */
static void mips64_jit_tcb_add_end(mips64_jit_tcb_t *b)
{
   mips64_set_pc(b,b->start_pc+(b->mips_trans_pos<<2));
   mips64_jit_tcb_push_epilog(b);
}

/* Record a patch to apply in a compiled block */
int mips64_jit_tcb_record_patch(mips64_jit_tcb_t *block,u_char *jit_ptr,
                                m_va_t vaddr)
{
   struct mips64_jit_patch_table *ipt = block->patch_table;
   struct mips64_insn_patch *patch;

   /* pc must be 32-bit aligned */
   if (vaddr & 0x03) {
      fprintf(stderr,"Block 0x%8.8llx: trying to record an invalid PC "
              "(0x%8.8llx) - mips_trans_pos=%d.\n",
              block->start_pc,vaddr,block->mips_trans_pos);
      return(-1);
   }

   if (!ipt || (ipt->cur_patch >= MIPS64_INSN_PATCH_TABLE_SIZE))
   {
      /* full table or no table, create a new one */
      ipt = malloc(sizeof(*ipt));
      if (!ipt) {
         fprintf(stderr,"Block 0x%8.8llx: unable to create patch table.\n",
                 block->start_pc);
         return(-1);
      }

      memset(ipt,0,sizeof(*ipt));
      ipt->next = block->patch_table;
      block->patch_table = ipt;
   }

#if DEBUG_BLOCK_PATCH
   printf("Block 0x%8.8llx: recording patch [JIT:%p->mips:0x%8.8llx], "
          "MTP=%d\n",block->start_pc,jit_ptr,vaddr,block->mips_trans_pos);
#endif

   patch = &ipt->patches[ipt->cur_patch];
   patch->jit_insn = jit_ptr;
   patch->mips_pc = vaddr;
   ipt->cur_patch++;   
   return(0);
}

/* Apply all patches */
static int mips64_jit_tcb_apply_patches(cpu_mips_t *cpu,
                                        mips64_jit_tcb_t *block)
{
   struct mips64_jit_patch_table *ipt;
   struct mips64_insn_patch *patch;
   u_char *jit_dst;
   int i;

   for(ipt=block->patch_table;ipt;ipt=ipt->next)
      for(i=0;i<ipt->cur_patch;i++) 
      {
         patch = &ipt->patches[i];
         jit_dst = mips64_jit_tcb_get_host_ptr(block,patch->mips_pc);

         if (jit_dst) {
#if DEBUG_BLOCK_PATCH
            printf("Block 0x%8.8llx: applying patch "
                   "[JIT:%p->mips:0x%8.8llx=JIT:%p]\n",
                   block->start_pc,patch->jit_insn,patch->mips_pc,jit_dst);
#endif
            mips64_jit_tcb_set_patch(patch->jit_insn,jit_dst);
         }
      }

   return(0);
}

/* Free the patch table */
static void mips64_jit_tcb_free_patches(mips64_jit_tcb_t *block)
{
   struct mips64_jit_patch_table *p,*next;

   for(p=block->patch_table;p;p=next) {
      next = p->next;
      free(p);
   }

   block->patch_table = NULL;
}

/* Adjust the JIT buffer if its size is not sufficient */
static int mips64_jit_tcb_adjust_buffer(cpu_mips_t *cpu,
                                        mips64_jit_tcb_t *block)
{
   insn_exec_page_t *new_buffer;

   if ((block->jit_ptr - block->jit_buffer->ptr) <= (MIPS_JIT_BUFSIZE - 512))
      return(0);

#if DEBUG_BLOCK_CHUNK  
   printf("Block 0x%llx: adjusting JIT buffer...\n",block->start_pc);
#endif

   if (block->jit_chunk_pos >= MIPS_JIT_MAX_CHUNKS) {
      fprintf(stderr,"Block 0x%llx: too many JIT chunks.\n",block->start_pc);
      return(-1);
   }

   if (!(new_buffer = exec_page_alloc(cpu)))
      return(-1);

   /* record the new exec page */
   block->jit_chunks[block->jit_chunk_pos++] = block->jit_buffer;
   block->jit_buffer = new_buffer;

   /* jump to the new exec page (link) */
   mips64_jit_tcb_set_jump(block->jit_ptr,new_buffer->ptr);
   block->jit_ptr = new_buffer->ptr;
   return(0);
}

/* Allocate an instruction block */
static inline mips64_jit_tcb_t *mips64_jit_tcb_alloc(cpu_mips_t *cpu)
{
   mips64_jit_tcb_t *p;

   if (cpu->tcb_free_list) {
      p = cpu->tcb_free_list;
      cpu->tcb_free_list = p->next;
   } else {
      if (!(p = malloc(sizeof(*p))))
         return NULL;
   }

   memset(p,0,sizeof(*p));
   return p;
}

/* Free an instruction block */
void mips64_jit_tcb_free(cpu_mips_t *cpu,mips64_jit_tcb_t *block,
                         int list_removal)
{
   int i;

   if (block) {
      if (list_removal) {
         /* Remove the block from the linked list */
         if (block->next)
            block->next->prev = block->prev;
         else
            cpu->tcb_last = block->prev;

         if (block->prev)
            block->prev->next = block->next;
         else
            cpu->tcb_list = block->next;
      }

      /* Free the patch tables */
      mips64_jit_tcb_free_patches(block);

      /* Free code pages */
      for(i=0;i<MIPS_JIT_MAX_CHUNKS;i++)
         exec_page_free(cpu,block->jit_chunks[i]);

      /* Free the current JIT buffer */
      exec_page_free(cpu,block->jit_buffer);

      /* Free the MIPS-to-native code mapping */
      free(block->jit_insn_ptr);

      /* Make the block return to the free list */
      block->next = cpu->tcb_free_list;
      cpu->tcb_free_list = block;
   }
}

/* Create an instruction block */
static mips64_jit_tcb_t *mips64_jit_tcb_create(cpu_mips_t *cpu,
                                               m_va_t vaddr)
{
   mips64_jit_tcb_t *block = NULL;

   if (!(block = mips64_jit_tcb_alloc(cpu)))
      goto err_block_alloc;

   block->start_pc = vaddr;

   /* Allocate the first JIT buffer */
   if (!(block->jit_buffer = exec_page_alloc(cpu)))
      goto err_jit_alloc;

   block->jit_ptr = block->jit_buffer->ptr;
   block->mips_code = cpu->mem_op_lookup(cpu,block->start_pc);

   if (!block->mips_code) {
   	/*TLB Exception*/
   	 if ((block->start_pc>=MIPS_KSSEG_BASE)||(block->start_pc<MIPS_KSEG1_BASE))
          {
              //BECAUSE TLB MISS. just return to main process.
              longjmp(run_jmp, 1); 
          }
   	 else
   	 	{
   	 	 fprintf(stderr,"%% No memory map for code execution at 0x%llx\n",
              block->start_pc);
      		goto err_lookup;
   	 	}
  }

   return block;

 err_lookup:
 err_jit_alloc:
   mips64_jit_tcb_free(cpu,block,FALSE);
 err_block_alloc:
   fprintf(stderr,"%% Unable to create instruction block for vaddr=0x%llx\n", 
           vaddr);
   return NULL;
}

/* Compile a MIPS instruction page */
static inline 
mips64_jit_tcb_t *mips64_jit_tcb_compile(cpu_mips_t *cpu,m_va_t vaddr)
{  
   mips64_jit_tcb_t *block;
   m_uint64_t page_addr;
   size_t len;

   page_addr = vaddr & ~(m_uint64_t)MIPS_MIN_PAGE_IMASK;

   if (unlikely(!(block = mips64_jit_tcb_create(cpu,page_addr)))) {
      fprintf(stderr,"insn_page_compile: unable to create JIT block.\n");
      return NULL;
   }

   /* Allocate the array used to convert MIPS code ptr to native code ptr */
   len = MIPS_MIN_PAGE_SIZE / sizeof(mips_insn_t);

   if (!(block->jit_insn_ptr = calloc(len,sizeof(u_char *)))) {
      fprintf(stderr,"insn_page_compile: unable to create JIT mappings.\n");
      goto error;
   }

   /* Emit native code for each instruction */
   block->mips_trans_pos = 0;

   while(block->mips_trans_pos < MIPS_INSN_PER_PAGE)
   {
      if (unlikely(( mips64_jit_fetch_and_emit(cpu,block,0)==-1))) {
         fprintf(stderr,"insn_page_compile: unable to fetch instruction.\n");
         goto error;
      }

#if DEBUG_BLOCK_COMPILE
      printf("Page 0x%8.8llx: emitted tag 0x%8.8x/0x%8.8x\n",
             block->start_pc,tag->mask,tag->value);
#endif

      mips64_jit_tcb_adjust_buffer(cpu,block);
   }

   mips64_jit_tcb_add_end(block);
   mips64_jit_tcb_apply_patches(cpu,block);
   mips64_jit_tcb_free_patches(block);

   /* Add the block to the linked list */
   block->next = cpu->tcb_list;
   block->prev = NULL;

   if (cpu->tcb_list)
      cpu->tcb_list->prev = block;
   else
      cpu->tcb_last = block;

   cpu->tcb_list = block;
   
   cpu->compiled_pages++;
   return block;

 error:
   mips64_jit_tcb_free(cpu,block,FALSE);
   return NULL;
}

/* Run a compiled MIPS instruction block */
static forced_inline 
void mips64_jit_tcb_run(cpu_mips_t *cpu,mips64_jit_tcb_t *block)
{

   if (unlikely(cpu->pc & 0x03)) {
      fprintf(stderr,"mips64_jit_tcb_run: Invalid PC 0x%llx.\n",cpu->pc);
      //mips64_dump_regs(cpu);
      //mips64_tlb_dump(cpu);
      cpu_stop(cpu);
      return;
   }
   /* Execute JIT compiled code */
   mips64_jit_tcb_exec(cpu,block);


}


void *mips64_jit_run_cpu(cpu_mips_t * cpu)
{
   mips_insn_t insn = 0;
   int res;
   m_uint32_t pc_hash;
   mips64_jit_tcb_t *block;


   cpu->cpu_thread_running = TRUE;
   current_cpu = cpu;

   mips64_init_host_alarm();
   
	setjmp(run_jmp) ;
 start_cpu:

   for (;;)
   {
      if (unlikely(cpu->state != CPU_STATE_RUNNING))
         break;

      if (unlikely((cpu->pause_request) & CPU_INTERRUPT_EXIT))
      {
         cpu->state = CPU_STATE_PAUSING;
         break;
      }


      /* Reset "zero register" (for safety) */
      cpu->gpr[0] = 0;

      /* Check IRQ */
      if (unlikely(cpu->irq_pending))
      {
         mips64_trigger_irq(cpu);
         //continue;
      }
		pc_hash = mips64_jit_get_pc_hash(cpu->pc);
      block = cpu->exec_blk_map[pc_hash];
      
            /* No block found, compile the page */
      if (unlikely(!block) || unlikely(!mips64_jit_tcb_match(cpu,block))) 
      {        
         if (block != NULL) {
            mips64_jit_tcb_free(cpu,block,TRUE);
            cpu->exec_blk_map[pc_hash] = NULL;
         }

         block = mips64_jit_tcb_compile(cpu,cpu->pc);
         if (unlikely(!block)) {
            fprintf(stderr,
                    "VM '%s': unable to compile block for CPU%u PC=0x%llx\n",
                    cpu->vm->name,cpu->id,cpu->pc);
            cpu_stop(cpu);
            break;
         }
	       block->acc_count++;
         cpu->exec_blk_map[pc_hash] = block;
      }
      mips64_jit_tcb_run(cpu,block);


   }

   while (cpu->cpu_thread_running)
   {
      switch (cpu->state)
      {
      case CPU_STATE_RUNNING:
         cpu->state = CPU_STATE_RUNNING;
         goto start_cpu;

      case CPU_STATE_HALTED:
         cpu->cpu_thread_running = FALSE;
         break;
      case CPU_STATE_RESTARTING:
         cpu->state = CPU_STATE_RESTARTING;
         /*Just waiting for cpu restart. */
         break;
      case CPU_STATE_PAUSING:
         /*main loop must wait for me. heihei :) */
         mips64_main_loop_wait(cpu, 0);
         cpu->state = CPU_STATE_RUNNING;
         cpu->pause_request &= ~CPU_INTERRUPT_EXIT;
         /*start cpu again */
         goto start_cpu;

      }
   }
   return NULL;
}





