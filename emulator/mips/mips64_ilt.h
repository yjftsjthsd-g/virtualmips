/*
 * Cisco router simulation platform.
 * Copyright (c) 2005,2006 Christophe Fillot (cf@utc.fr)
 *
 */

  /*
   * Copyright (C) yajin 2008 <yajinzhou@gmail.com >
   *     
   * This file is part of the virtualmips distribution. 
   * See LICENSE file for terms of the license. 
   *
   */


/*instruction lookup routine from dynamips*/

/* ILT */
static void *mips64_exec_get_insn(int index)
{
   return (&mips64_exec_tags[index]);
}

static int mips64_exec_chk_lo(struct mips64_insn_exec_tag *tag, int value)
{
   return ((value & tag->mask) == (tag->value & 0xFFFF));
}

static int mips64_exec_chk_hi(struct mips64_insn_exec_tag *tag, int value)
{
   return ((value & (tag->mask >> 16)) == (tag->value >> 16));
}

/* Initialize instruction lookup table */
void mips64_exec_create_ilt(char *ilt_name)
{
   int i, count;

   for (i = 0, count = 0; mips64_exec_tags[i].exec; i++)
      count++;

   ilt = ilt_create(ilt_name, count,
                    (ilt_get_insn_cbk_t) mips64_exec_get_insn,
                    (ilt_check_cbk_t) mips64_exec_chk_lo, (ilt_check_cbk_t) mips64_exec_chk_hi);
}

/* Unknown opcode */
static int mips64_exec_unknown(cpu_mips_t * cpu, mips_insn_t insn)
{
   printf("MIPS64: unknown opcode 0x%8.8x at pc = 0x%" LL "x\n", insn, cpu->pc);
   exit(-1);
   return (0);
}


/* ADD */
static int mips64_exec_ADD(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   m_reg_t res;

   /* TODO: Exception handling */
   res = (m_reg_t) (m_uint32_t) cpu->gpr[rs] + (m_uint32_t) cpu->gpr[rt];
   cpu->gpr[rd] = sign_extend(res, 32);
   return (0);
}

/* ADDI */
static int mips64_exec_ADDI(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int imm = bits(insn, 0, 15);
   m_uint32_t res, val = sign_extend(imm, 16);

   /* TODO: Exception handling */
   res = (m_uint32_t) cpu->gpr[rs] + val;
   cpu->gpr[rt] = sign_extend(res, 32);
   return (0);
}

/* ADDIU */
static int mips64_exec_ADDIU(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int imm = bits(insn, 0, 15);
   m_uint32_t res, val = sign_extend(imm, 16);

   res = (m_uint32_t) cpu->gpr[rs] + val;
   cpu->gpr[rt] = sign_extend(res, 32);


   return (0);
}

/* ADDU */
static int mips64_exec_ADDU(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   m_uint32_t res;

   res = (m_uint32_t) cpu->gpr[rs] + (m_uint32_t) cpu->gpr[rt];
   cpu->gpr[rd] = sign_extend(res, 32);
   return (0);
}

/* AND */
static int mips64_exec_AND(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);

   cpu->gpr[rd] = cpu->gpr[rs] & cpu->gpr[rt];
   return (0);
}

/* ANDI */
static int mips64_exec_ANDI(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int imm = bits(insn, 0, 15);

   cpu->gpr[rt] = cpu->gpr[rs] & imm;
   return (0);
}

/* B (Branch, virtual instruction) */
static int mips64_exec_B(cpu_mips_t * cpu, mips_insn_t insn)
{
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* exec the instruction in the delay slot */
   int ins_res = mips64_exec_bdslot(cpu);

   if (likely(!ins_res))
   {
      /* set the new pc in cpu structure */
      cpu->pc = new_pc;

   }

   return (1);
}

/* BAL (Branch And Link, virtual instruction) */
static int mips64_exec_BAL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* set the return address (instruction after the delay slot) */
   cpu->gpr[MIPS_GPR_RA] = cpu->pc + 8;

   /* exec the instruction in the delay slot */
   int ins_res = mips64_exec_bdslot(cpu);

   if (likely(!ins_res))
   {
      /* set the new pc in cpu structure */
      cpu->pc = new_pc;

   }
   return (1);
}

/* BEQ (Branch On Equal) */
static int mips64_exec_BEQ(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* take the branch if gpr[rs] == gpr[rt] */
   res = (cpu->gpr[rs] == cpu->gpr[rt]);

   /* exec the instruction in the delay slot */
   int ins_res = mips64_exec_bdslot(cpu);
    /**/ if (likely(!ins_res))
   {
      if (res)
         cpu->pc = new_pc;
      else
         cpu->pc += 8;
   }

   return (1);
}

/* BEQL (Branch On Equal Likely) */
static int mips64_exec_BEQL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* take the branch if gpr[rs] == gpr[rt] */
   res = (cpu->gpr[rs] == cpu->gpr[rt]);

   /* take the branch if the test result is true */
   if (res)
   {
      int ins_res = mips64_exec_bdslot(cpu);
      if (likely(!ins_res))
         cpu->pc = new_pc;
   }
   else
      cpu->pc += 8;

   return (1);
}

/* BEQZ (Branch On Equal Zero) - Virtual Instruction */
static int mips64_exec_BEQZ(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* take the branch if gpr[rs] == 0 */
   res = (cpu->gpr[rs] == 0);

   /* exec the instruction in the delay slot */
   int ins_res = mips64_exec_bdslot(cpu);
    /**/ if (likely(!ins_res))

   {
      /* take the branch if the test result is true */

      if (res)
         cpu->pc = new_pc;
      else
         cpu->pc += 8;


   }
   return (1);
}

/* BNEZ (Branch On Not Equal Zero) - Virtual Instruction */
static int mips64_exec_BNEZ(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* take the branch if gpr[rs] != 0 */
   res = (cpu->gpr[rs] != 0);

   /* exec the instruction in the delay slot */
   int ins_res = mips64_exec_bdslot(cpu);

   if (likely(!ins_res))
   {
      /* take the branch if the test result is true */
      if (res)
         cpu->pc = new_pc;
      else
         cpu->pc += 8;
   }

   return (1);
}

/* BGEZ (Branch On Greater or Equal Than Zero) */
static int mips64_exec_BGEZ(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* take the branch if gpr[rs] >= 0 */
   res = ((m_ireg_t) cpu->gpr[rs] >= 0);

   /* exec the instruction in the delay slot */
   /* exec the instruction in the delay slot */
   int ins_res = mips64_exec_bdslot(cpu);

   if (likely(!ins_res))

   {
      /* take the branch if the test result is true */
      if (res)
         cpu->pc = new_pc;
      else
         cpu->pc += 8;


   }


   return (1);
}

/* BGEZAL (Branch On Greater or Equal Than Zero And Link) */
static int mips64_exec_BGEZAL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* set the return address (instruction after the delay slot) */
   cpu->gpr[MIPS_GPR_RA] = cpu->pc + 8;

   /* take the branch if gpr[rs] >= 0 */
   res = ((m_ireg_t) cpu->gpr[rs] >= 0);

   /* exec the instruction in the delay slot */
   int ins_res = mips64_exec_bdslot(cpu);

   if (likely(!ins_res))

   {
      /* take the branch if the test result is true */
      if (res)
         cpu->pc = new_pc;
      else
         cpu->pc += 8;
   }


   return (1);
}

/* BGEZALL (Branch On Greater or Equal Than Zero And Link Likely) */
static int mips64_exec_BGEZALL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* set the return address (instruction after the delay slot) */
   cpu->gpr[MIPS_GPR_RA] = cpu->pc + 8;

   /* take the branch if gpr[rs] >= 0 */
   res = ((m_ireg_t) cpu->gpr[rs] >= 0);

   /* take the branch if the test result is true */
   if (res)
   {
      mips64_exec_bdslot(cpu);
      cpu->pc = new_pc;
   }
   else
      cpu->pc += 8;

   return (1);
}

/* BGEZL (Branch On Greater or Equal Than Zero Likely) */
static int mips64_exec_BGEZL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* take the branch if gpr[rs] >= 0 */
   res = ((m_ireg_t) cpu->gpr[rs] >= 0);

   /* take the branch if the test result is true */
   if (res)
   {
      mips64_exec_bdslot(cpu);
      cpu->pc = new_pc;
   }
   else
      cpu->pc += 8;

   return (1);
}

/* BGTZ (Branch On Greater Than Zero) */
static int mips64_exec_BGTZ(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* take the branch if gpr[rs] > 0 */
   res = ((m_ireg_t) cpu->gpr[rs] > 0);

   /* exec the instruction in the delay slot */
   int ins_res = mips64_exec_bdslot(cpu);

   if (likely(!ins_res))
   {
      /* take the branch if the test result is true */
      if (res)
         cpu->pc = new_pc;
      else
         cpu->pc += 8;
   }

   return (1);
}

/* BGTZL (Branch On Greater Than Zero Likely) */
static int mips64_exec_BGTZL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* take the branch if gpr[rs] > 0 */
   res = ((m_ireg_t) cpu->gpr[rs] > 0);

   /* take the branch if the test result is true */
   if (res)
   {
      mips64_exec_bdslot(cpu);
      cpu->pc = new_pc;
   }
   else
      cpu->pc += 8;

   return (1);
}

/* BLEZ (Branch On Less or Equal Than Zero) */
static int mips64_exec_BLEZ(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* take the branch if gpr[rs] <= 0 */
   res = ((m_ireg_t) cpu->gpr[rs] <= 0);

   /* exec the instruction in the delay slot */
   int ins_res = mips64_exec_bdslot(cpu);

   if (likely(!ins_res))
   {
      /* take the branch if the test result is true */
      if (res)
         cpu->pc = new_pc;
      else
         cpu->pc += 8;
   }

   return (1);
}

/* BLEZL (Branch On Less or Equal Than Zero Likely) */
static int mips64_exec_BLEZL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* take the branch if gpr[rs] <= 0 */
   res = ((m_ireg_t) cpu->gpr[rs] <= 0);

   /* take the branch if the test result is true */
   if (res)
   {
      mips64_exec_bdslot(cpu);
      cpu->pc = new_pc;
   }
   else
      cpu->pc += 8;

   return (1);
}

/* BLTZ (Branch On Less Than Zero) */
static int mips64_exec_BLTZ(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* take the branch if gpr[rs] < 0 */
   res = ((m_ireg_t) cpu->gpr[rs] < 0);

   /* exec the instruction in the delay slot */
   int ins_res = mips64_exec_bdslot(cpu);

   if (likely(!ins_res))

   {
      /* take the branch if the test result is true */
      if (res)
         cpu->pc = new_pc;
      else
         cpu->pc += 8;

   }

   return (1);
}

/* BLTZAL (Branch On Less Than Zero And Link) */
static int mips64_exec_BLTZAL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* set the return address (instruction after the delay slot) */
   cpu->gpr[MIPS_GPR_RA] = cpu->pc + 8;

   /* take the branch if gpr[rs] < 0 */
   res = ((m_ireg_t) cpu->gpr[rs] < 0);

   /* exec the instruction in the delay slot */
   int ins_res = mips64_exec_bdslot(cpu);

   if (likely(!ins_res))
   {
      /* take the branch if the test result is true */
      if (res)
         cpu->pc = new_pc;
      else
         cpu->pc += 8;
   }

   return (1);
}

/* BLTZALL (Branch On Less Than Zero And Link Likely) */
static int mips64_exec_BLTZALL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* set the return address (instruction after the delay slot) */
   cpu->gpr[MIPS_GPR_RA] = cpu->pc + 8;

   /* take the branch if gpr[rs] < 0 */
   res = ((m_ireg_t) cpu->gpr[rs] < 0);

   /* take the branch if the test result is true */
   if (res)
   {
      mips64_exec_bdslot(cpu);
      cpu->pc = new_pc;
   }
   else
      cpu->pc += 8;

   return (1);
}

/* BLTZL (Branch On Less Than Zero Likely) */
static int mips64_exec_BLTZL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* take the branch if gpr[rs] < 0 */
   res = ((m_ireg_t) cpu->gpr[rs] < 0);

   /* take the branch if the test result is true */
   if (res)
   {
      mips64_exec_bdslot(cpu);
      cpu->pc = new_pc;
   }
   else
      cpu->pc += 8;

   return (1);
}

/* BNE (Branch On Not Equal) */
static int mips64_exec_BNE(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);


   /* take the branch if gpr[rs] != gpr[rt] */
   res = (cpu->gpr[rs] != cpu->gpr[rt]);

   /* exec the instruction in the delay slot */
   int ins_res = mips64_exec_bdslot(cpu);

   if (likely(!ins_res))
   {
      /* take the branch if the test result is true */
      if (res)
         cpu->pc = new_pc;
      else
         cpu->pc += 8;
   }

   return (1);
}

/* BNEL (Branch On Not Equal Likely) */
static int mips64_exec_BNEL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);
   m_va_t new_pc;
   int res;

   /* compute the new pc */
   new_pc = (cpu->pc + 4) + sign_extend(offset << 2, 18);

   /* take the branch if gpr[rs] != gpr[rt] */
   res = (cpu->gpr[rs] != cpu->gpr[rt]);


   /* take the branch if the test result is true */
   if (res)
   {


      mips64_exec_bdslot(cpu);
      cpu->pc = new_pc;
   }
   else
      cpu->pc += 8;

   return (1);
}

/* BREAK */
static int mips64_exec_BREAK(cpu_mips_t * cpu, mips_insn_t insn)
{
   u_int code = bits(insn, 6, 25);

   mips64_exec_break(cpu, code);
   return (1);
}

/* CACHE */
static int mips64_exec_CACHE(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int op = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_CACHE, base, offset, op, FALSE));
}



/* CLZ  */
static int mips64_exec_CLZ(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rd = bits(insn, 11, 15);
   int i;
   m_uint32_t val;
   val = 32;
   for (i = 31; i >= 0; i--)
   {
      if (cpu->gpr[rs] & (1 << i))
      {
         val = 31 - i;
         break;
      }
   }
   cpu->gpr[rd] = val;
   return (0);

}


/* CFC0 */
/*static    int mips64_exec_CFC0(cpu_mips_t *cpu,mips_insn_t insn)
{	
   int rt = bits(insn,16,20);
   int rd = bits(insn,11,15);

   mips64_cp0_exec_cfc0(cpu,rt,rd);
   return(0);
}*/

/* CTC0 */
/*static    int mips64_exec_CTC0(cpu_mips_t *cpu,mips_insn_t insn)
{	
   int rt = bits(insn,16,20);
   int rd = bits(insn,11,15);

   mips64_cp0_exec_ctc0(cpu,rt,rd);
   return(0);
}*/



/* DIV */
static int mips64_exec_DIV(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);

   cpu->lo = (m_int32_t) cpu->gpr[rs] / (m_int32_t) cpu->gpr[rt];
   cpu->hi = (m_int32_t) cpu->gpr[rs] % (m_int32_t) cpu->gpr[rt];

   cpu->lo = sign_extend(cpu->lo, 32);
   cpu->hi = sign_extend(cpu->hi, 32);
   return (0);
}

/* DIVU */
static int mips64_exec_DIVU(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);

   if (cpu->gpr[rt] == 0)
      return (0);

   cpu->lo = (m_uint32_t) cpu->gpr[rs] / (m_uint32_t) cpu->gpr[rt];
   cpu->hi = (m_uint32_t) cpu->gpr[rs] % (m_uint32_t) cpu->gpr[rt];

   cpu->lo = sign_extend(cpu->lo, 32);
   cpu->hi = sign_extend(cpu->hi, 32);
   return (0);
}



/* ERET */
static int mips64_exec_ERET(cpu_mips_t * cpu, mips_insn_t insn)
{
   mips64_exec_eret(cpu);
   return (1);
}

/* J */
static int mips64_exec_J(cpu_mips_t * cpu, mips_insn_t insn)
{
   u_int instr_index = bits(insn, 0, 25);
   m_va_t new_pc;

   /* compute the new pc */
   new_pc = cpu->pc & ~((1 << 28) - 1);
   new_pc |= instr_index << 2;

   /* exec the instruction in the delay slot */
   int ins_res = mips64_exec_bdslot(cpu);
   if (likely(!ins_res))
      cpu->pc = new_pc;
   return (1);
}

/* JAL */
static int mips64_exec_JAL(cpu_mips_t * cpu, mips_insn_t insn)
{
   u_int instr_index = bits(insn, 0, 25);
   m_va_t new_pc;

   /* compute the new pc */
   new_pc = cpu->pc & ~((1 << 28) - 1);
   new_pc |= instr_index << 2;

   /* set the return address (instruction after the delay slot) */
   cpu->gpr[MIPS_GPR_RA] = cpu->pc + 8;

   int ins_res = mips64_exec_bdslot(cpu);
   if (likely(!ins_res))
      cpu->pc = new_pc;

   return (1);
}

/* JALR */
static int mips64_exec_JALR(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rd = bits(insn, 11, 15);
   m_va_t new_pc;

   /* set the return pc (instruction after the delay slot) in GPR[rd] */
   cpu->gpr[rd] = cpu->pc + 8;

   /* get the new pc */
   new_pc = cpu->gpr[rs];

   int ins_res = mips64_exec_bdslot(cpu);
   if (likely(!ins_res))
      cpu->pc = new_pc;
   return (1);
}

/* JR */
static int mips64_exec_JR(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   m_va_t new_pc;

   /* get the new pc */
   new_pc = cpu->gpr[rs];

   int ins_res = mips64_exec_bdslot(cpu);
   if (likely(!ins_res))
      cpu->pc = new_pc;
   return (1);
}

/* LB (Load Byte) */
static int mips64_exec_LB(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_LB, base, offset, rt, TRUE));
}

/* LBU (Load Byte Unsigned) */
static int mips64_exec_LBU(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_LBU, base, offset, rt, TRUE));
}

/* LH (Load Half-Word) */
static int mips64_exec_LH(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_LH, base, offset, rt, TRUE));
}

/* LHU (Load Half-Word Unsigned) */
static int mips64_exec_LHU(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_LHU, base, offset, rt, TRUE));
}

/* LI (virtual) */
static int mips64_exec_LI(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rt = bits(insn, 16, 20);
   int imm = bits(insn, 0, 15);

   cpu->gpr[rt] = sign_extend(imm, 16);
   return (0);
}

/* LL (Load Linked) */
static int mips64_exec_LL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_LL, base, offset, rt, TRUE));
}

/* LUI */
static int mips64_exec_LUI(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rt = bits(insn, 16, 20);
   int imm = bits(insn, 0, 15);

   cpu->gpr[rt] = sign_extend(imm, 16) << 16;
   return (0);
}

/* LW (Load Word) */
static int mips64_exec_LW(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_LW, base, offset, rt, TRUE));
}

/* LWL (Load Word Left) */
static int mips64_exec_LWL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_LWL, base, offset, rt, TRUE));
}

/* LWR (Load Word Right) */
static int mips64_exec_LWR(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_LWR, base, offset, rt, TRUE));
}

/* LWU (Load Word Unsigned) */
static int mips64_exec_LWU(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_LWU, base, offset, rt, TRUE));
}

static int mips64_exec_MAD(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   m_int64_t val, temp;

   val = (m_int32_t) (m_int32_t) cpu->gpr[rs];
   val *= (m_int32_t) (m_int32_t) cpu->gpr[rt];

   temp = cpu->hi;
   temp = temp << 32;
   temp += cpu->lo;
   val += temp;

   cpu->lo = sign_extend(val, 32);
   cpu->hi = sign_extend(val >> 32, 32);
   return (0);
}

static int mips64_exec_MADU(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   m_int64_t val, temp;

   val = (m_uint32_t) (m_uint32_t) cpu->gpr[rs];
   val *= (m_uint32_t) (m_uint32_t) cpu->gpr[rt];

   temp = cpu->hi;
   temp = temp << 32;
   temp += cpu->lo;
   val += temp;


   cpu->lo = sign_extend(val, 32);
   cpu->hi = sign_extend(val >> 32, 32);
   return (0);
}


/* MFC0 */
static int mips64_exec_MFC0(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   int sel = bits(insn, 0, 2);
   //mfc rt,rd

   mips64_cp0_exec_mfc0(cpu, rt, rd, sel);
   return (0);
}


/* MFHI */
static int mips64_exec_MFHI(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rd = bits(insn, 11, 15);

   if (rd)
      cpu->gpr[rd] = cpu->hi;
   return (0);
}

/* MFLO */
static int mips64_exec_MFLO(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rd = bits(insn, 11, 15);

   if (rd)
      cpu->gpr[rd] = cpu->lo;
   return (0);
}

/* MOVE (virtual instruction, real: ADDU) */
static int mips64_exec_MOVE(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rd = bits(insn, 11, 15);

   cpu->gpr[rd] = sign_extend(cpu->gpr[rs], 32);
   return (0);
}

static int mips64_exec_MOVEN(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rd = bits(insn, 11, 15);
   int rt = bits(insn, 16, 20);

   // printf("pc %x rs %x rd %x rt %x\n",cpu->pc,rs,rd,rt);
   if ((cpu->gpr[rt]) != 0)
      cpu->gpr[rd] = sign_extend(cpu->gpr[rs], 32);
   return (0);
}

static int mips64_exec_MOVEZ(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rd = bits(insn, 11, 15);
   int rt = bits(insn, 16, 20);


   if ((cpu->gpr[rt]) == 0)
      cpu->gpr[rd] = sign_extend(cpu->gpr[rs], 32);
   return (0);
}

static int mips64_exec_MSUB(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   m_int64_t val, temp;

   val = (m_int32_t) (m_int32_t) cpu->gpr[rs];
   val *= (m_int32_t) (m_int32_t) cpu->gpr[rt];

   temp = cpu->hi;
   temp = temp << 32;
   temp += cpu->lo;

   temp -= val;
   //val += temp;

   cpu->lo = sign_extend(temp, 32);
   cpu->hi = sign_extend(temp >> 32, 32);
   return (0);


}

static int mips64_exec_MSUBU(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   m_int64_t val, temp;

   val = (m_uint32_t) (m_uint32_t) cpu->gpr[rs];
   val *= (m_uint32_t) (m_uint32_t) cpu->gpr[rt];

   temp = cpu->hi;
   temp = temp << 32;
   temp += cpu->lo;

   temp -= val;
   //val += temp;

   cpu->lo = sign_extend(temp, 32);
   cpu->hi = sign_extend(temp >> 32, 32);
   return (0);


}

/* MTC0 */
static int mips64_exec_MTC0(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   int sel = bits(insn, 0, 2);
   //printf("cpu->pc %x insn %x\n",cpu->pc,insn);
   mips64_cp0_exec_mtc0(cpu, rt, rd, sel);
   return (0);
}




/* MTHI */
static int mips64_exec_MTHI(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);

   cpu->hi = cpu->gpr[rs];
   return (0);
}

/* MTLO */
static int mips64_exec_MTLO(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);

   cpu->lo = cpu->gpr[rs];
   return (0);
}

/* MUL */
static int mips64_exec_MUL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   m_int32_t val;

   /* note: after this instruction, HI/LO regs are undefined */
   val = (m_int32_t) cpu->gpr[rs] * (m_int32_t) cpu->gpr[rt];
   cpu->gpr[rd] = sign_extend(val, 32);
   return (0);
}

/* MULT */
static int mips64_exec_MULT(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   m_int64_t val;

   val = (m_int64_t) (m_int32_t) cpu->gpr[rs];
   val *= (m_int64_t) (m_int32_t) cpu->gpr[rt];

   cpu->lo = sign_extend(val, 32);
   cpu->hi = sign_extend(val >> 32, 32);
   return (0);
}

/* MULTU */
static int mips64_exec_MULTU(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   m_int64_t val;               //must be 64 bit. not m_reg_t !!!

   val = (m_reg_t) (m_uint32_t) cpu->gpr[rs];
   val *= (m_reg_t) (m_uint32_t) cpu->gpr[rt];
   cpu->lo = sign_extend(val, 32);
   cpu->hi = sign_extend(val >> 32, 32);
   return (0);
}

/* NOP */
static int mips64_exec_NOP(cpu_mips_t * cpu, mips_insn_t insn)
{
   return (0);
}

/* NOR */
static int mips64_exec_NOR(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);

   cpu->gpr[rd] = ~(cpu->gpr[rs] | cpu->gpr[rt]);
   return (0);
}

/* OR */
static int mips64_exec_OR(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);

   cpu->gpr[rd] = cpu->gpr[rs] | cpu->gpr[rt];
   return (0);
}

/* ORI */
static int mips64_exec_ORI(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int imm = bits(insn, 0, 15);

   cpu->gpr[rt] = cpu->gpr[rs] | imm;
   return (0);
}

/* PREF */
static int mips64_exec_PREF(cpu_mips_t * cpu, mips_insn_t insn)
{
   return (0);
}

/* PREFI */
static int mips64_exec_PREFI(cpu_mips_t * cpu, mips_insn_t insn)
{
   return (0);
}

/* SB (Store Byte) */
static int mips64_exec_SB(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);



   return (mips64_exec_memop2(cpu, MIPS_MEMOP_SB, base, offset, rt, FALSE));
}

/* SC (Store Conditional) */
static int mips64_exec_SC(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_SC, base, offset, rt, TRUE));
}


/* SH (Store Half-Word) */
static int mips64_exec_SH(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_SH, base, offset, rt, FALSE));
}

/* SLL */
static int mips64_exec_SLL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   int sa = bits(insn, 6, 10);
   m_uint32_t res;

   res = (m_uint32_t) cpu->gpr[rt] << sa;
   cpu->gpr[rd] = sign_extend(res, 32);
   return (0);
}

/* SLLV */
static int mips64_exec_SLLV(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   m_uint32_t res;

   res = (m_uint32_t) cpu->gpr[rt] << (cpu->gpr[rs] & 0x1f);
   cpu->gpr[rd] = sign_extend(res, 32);
   return (0);
}

/* SLT */
static int mips64_exec_SLT(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);

   if ((m_ireg_t) cpu->gpr[rs] < (m_ireg_t) cpu->gpr[rt])
      cpu->gpr[rd] = 1;
   else
      cpu->gpr[rd] = 0;

   return (0);
}

/* SLTI */
static int mips64_exec_SLTI(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int imm = bits(insn, 0, 15);
   m_ireg_t val = sign_extend(imm, 16);

   if ((m_ireg_t) cpu->gpr[rs] < val)
      cpu->gpr[rt] = 1;
   else
      cpu->gpr[rt] = 0;

   return (0);
}

/* SLTIU */
static int mips64_exec_SLTIU(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int imm = bits(insn, 0, 15);
   m_reg_t val = sign_extend(imm, 16);

   if (cpu->gpr[rs] < val)
      cpu->gpr[rt] = 1;
   else
      cpu->gpr[rt] = 0;

   return (0);
}

/* SLTU */
static int mips64_exec_SLTU(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);


   if (cpu->gpr[rs] < cpu->gpr[rt])
      cpu->gpr[rd] = 1;
   else
      cpu->gpr[rd] = 0;

   return (0);
}

/* SRA */
static int mips64_exec_SRA(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   int sa = bits(insn, 6, 10);
   m_int32_t res;

   res = (m_int32_t) cpu->gpr[rt] >> sa;
   cpu->gpr[rd] = sign_extend(res, 32);
   return (0);
}

/* SRAV */
static int mips64_exec_SRAV(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   m_int32_t res;

   res = (m_int32_t) cpu->gpr[rt] >> (cpu->gpr[rs] & 0x1f);
   cpu->gpr[rd] = sign_extend(res, 32);
   return (0);
}

/* SRL */
static int mips64_exec_SRL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   int sa = bits(insn, 6, 10);
   m_uint32_t res;

   res = (m_uint32_t) cpu->gpr[rt] >> sa;
   cpu->gpr[rd] = sign_extend(res, 32);
   return (0);
}

/* SRLV */
static int mips64_exec_SRLV(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   m_uint32_t res;

   res = (m_uint32_t) cpu->gpr[rt] >> (cpu->gpr[rs] & 0x1f);
   cpu->gpr[rd] = sign_extend(res, 32);
   return (0);
}

/* SUB */
static int mips64_exec_SUB(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   m_uint32_t res;

   /* TODO: Exception handling */
   res = (m_uint32_t) cpu->gpr[rs] - (m_uint32_t) cpu->gpr[rt];
   cpu->gpr[rd] = sign_extend(res, 32);
   return (0);
}

/* SUBU */
static int mips64_exec_SUBU(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   m_uint32_t res;
   res = (m_uint32_t) cpu->gpr[rs] - (m_uint32_t) cpu->gpr[rt];
   cpu->gpr[rd] = sign_extend(res, 32);

   return (0);
}

/* SW (Store Word) */
static int mips64_exec_SW(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_SW, base, offset, rt, FALSE));
}

/* SWL (Store Word Left) */
static int mips64_exec_SWL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_SWL, base, offset, rt, FALSE));
}

/* SWR (Store Word Right) */
static int mips64_exec_SWR(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_SWR, base, offset, rt, FALSE));
}

/* SYNC */
static int mips64_exec_SYNC(cpu_mips_t * cpu, mips_insn_t insn)
{
   return (0);
}

/* SYSCALL */
static int mips64_exec_SYSCALL(cpu_mips_t * cpu, mips_insn_t insn)
{
   mips64_exec_syscall(cpu);
   return (1);
}

/* TEQ (Trap if Equal) */
static int mips64_exec_TEQ(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);

   if (unlikely(cpu->gpr[rs] == cpu->gpr[rt]))
   {
      mips64_trigger_trap_exception(cpu);
      return (1);
   }

   return (0);
}

/* TEQI (Trap if Equal Immediate) */
static int mips64_exec_TEQI(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int imm = bits(insn, 0, 15);
   m_reg_t val = sign_extend(imm, 16);

   if (unlikely(cpu->gpr[rs] == val))
   {
      mips64_trigger_trap_exception(cpu);
      return (1);
   }

   return (0);
}

/* TLBP */
static int mips64_exec_TLBP(cpu_mips_t * cpu, mips_insn_t insn)
{
   mips64_cp0_exec_tlbp(cpu);
   return (0);
}

/* TLBR */
static int mips64_exec_TLBR(cpu_mips_t * cpu, mips_insn_t insn)
{
   mips64_cp0_exec_tlbr(cpu);
   return (0);
}

/* TLBWI */
static int mips64_exec_TLBWI(cpu_mips_t * cpu, mips_insn_t insn)
{
   mips64_cp0_exec_tlbwi(cpu);
   return (0);
}

/* TLBWR */
static int mips64_exec_TLBWR(cpu_mips_t * cpu, mips_insn_t insn)
{
   mips64_cp0_exec_tlbwr(cpu);
   return (0);
}

/* TLBWR */
static int mips64_exec_TNE(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);

   if ((m_ireg_t) cpu->gpr[rs] != (m_ireg_t) cpu->gpr[rt])
   {
      /*take a trap */
      mips64_trigger_trap_exception(cpu);
      return (1);
   }
   else
      return (0);
}



/* wait */
static int mips64_exec_WAIT(cpu_mips_t * cpu, mips_insn_t insn)
{
   return (0);
}

/* XOR */
static int mips64_exec_XOR(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);

   cpu->gpr[rd] = cpu->gpr[rs] ^ cpu->gpr[rt];
   return (0);
}

/* XORI */
static int mips64_exec_XORI(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int imm = bits(insn, 0, 15);

   cpu->gpr[rt] = cpu->gpr[rs] ^ imm;
   return (0);
}

#if MIPS_64
/* DADDIU */
static int mips64_exec_DADDIU(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int imm = bits(insn, 0, 15);
   m_reg_t val = sign_extend(imm, 16);

   cpu->gpr[rt] = cpu->gpr[rs] + val;
   return (0);
}

/* DADDU: rd = rs + rt */
static int mips64_exec_DADDU(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);

   cpu->gpr[rd] = cpu->gpr[rs] + cpu->gpr[rt];
   return (0);
}

/* DSLL */
static int mips64_exec_DSLL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   int sa = bits(insn, 6, 10);

   cpu->gpr[rd] = cpu->gpr[rt] << sa;
   return (0);
}

/* DSLL32 */
static int mips64_exec_DSLL32(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   int sa = bits(insn, 6, 10);

   cpu->gpr[rd] = cpu->gpr[rt] << (32 + sa);
   return (0);
}

/* DSLLV */
static int mips64_exec_DSLLV(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);

   cpu->gpr[rd] = cpu->gpr[rt] << (cpu->gpr[rs] & 0x3f);
   return (0);
}

/* DSRA */
static int mips64_exec_DSRA(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   int sa = bits(insn, 6, 10);

   cpu->gpr[rd] = (m_int64_t) cpu->gpr[rt] >> sa;
   return (0);
}

/* DSRA32 */
static int mips64_exec_DSRA32(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   int sa = bits(insn, 6, 10);

   cpu->gpr[rd] = (m_int64_t) cpu->gpr[rt] >> (32 + sa);
   return (0);
}

/* DSRAV */
static int mips64_exec_DSRAV(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);

   cpu->gpr[rd] = (m_int64_t) cpu->gpr[rt] >> (cpu->gpr[rs] & 0x3f);
   return (0);
}

/* DSRL */
static int mips64_exec_DSRL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   int sa = bits(insn, 6, 10);

   cpu->gpr[rd] = cpu->gpr[rt] >> sa;
   return (0);
}

/* DSRL32 */
static int mips64_exec_DSRL32(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);
   int sa = bits(insn, 6, 10);

   cpu->gpr[rd] = cpu->gpr[rt] >> (32 + sa);
   return (0);
}

/* DSRLV */
static int mips64_exec_DSRLV(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);

   cpu->gpr[rd] = cpu->gpr[rt] >> (cpu->gpr[rs] & 0x3f);
   return (0);
}

/* DSUBU */
static int mips64_exec_DSUBU(cpu_mips_t * cpu, mips_insn_t insn)
{
   int rs = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int rd = bits(insn, 11, 15);

   cpu->gpr[rd] = cpu->gpr[rs] - cpu->gpr[rt];
   return (0);
}

/* LD (Load Double-Word) */
static int mips64_exec_LD(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_LD, base, offset, rt, TRUE));
}

/* LDL (Load Double-Word Left) */
static int mips64_exec_LDL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_LDL, base, offset, rt, TRUE));
}

/* LDR (Load Double-Word Right) */
static int mips64_exec_LDR(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_LDR, base, offset, rt, TRUE));
}


/* SD (Store Double-Word) */
static int mips64_exec_SD(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_SD, base, offset, rt, FALSE));
}

/* SDL (Store Double-Word Left) */
static int mips64_exec_SDL(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_SDL, base, offset, rt, FALSE));
}

/* SDR (Store Double-Word Right) */
static int mips64_exec_SDR(cpu_mips_t * cpu, mips_insn_t insn)
{
   int base = bits(insn, 21, 25);
   int rt = bits(insn, 16, 20);
   int offset = bits(insn, 0, 15);

   return (mips64_exec_memop2(cpu, MIPS_MEMOP_SDR, base, offset, rt, FALSE));
}


#endif


#if SOFT_FPU
/*ALL FPU INSTRUCTION*/
static int mips64_exec_SOFTFPU(cpu_mips_t * cpu, mips_insn_t insn)
{

   mips64_exec_soft_fpu(cpu);
   return (1);
}
#endif


/* MIPS instruction array */
static struct mips64_insn_exec_tag mips64_exec_tags[] = {
   {"li", mips64_exec_LI, 0xffe00000, 0x24000000, 1, 16},
   {"move", mips64_exec_MOVE, 0xfc1f07ff, 0x00000021, 1, 15},
   {"b", mips64_exec_B, 0xffff0000, 0x10000000, 0, 10},
   {"bal", mips64_exec_BAL, 0xffff0000, 0x04110000, 0, 10},
   {"beqz", mips64_exec_BEQZ, 0xfc1f0000, 0x10000000, 0, 9},
   {"bnez", mips64_exec_BNEZ, 0xfc1f0000, 0x14000000, 0, 9},
   {"add", mips64_exec_ADD, 0xfc0007ff, 0x00000020, 1, 3},
   {"addi", mips64_exec_ADDI, 0xfc000000, 0x20000000, 1, 6},
   {"addiu", mips64_exec_ADDIU, 0xfc000000, 0x24000000, 1, 6},
   {"addu", mips64_exec_ADDU, 0xfc0007ff, 0x00000021, 1, 3},
   {"and", mips64_exec_AND, 0xfc0007ff, 0x00000024, 1, 3},
   {"andi", mips64_exec_ANDI, 0xfc000000, 0x30000000, 1, 5},
   {"beq", mips64_exec_BEQ, 0xfc000000, 0x10000000, 0, 8},
   {"beql", mips64_exec_BEQL, 0xfc000000, 0x50000000, 0, 8},
   {"bgez", mips64_exec_BGEZ, 0xfc1f0000, 0x04010000, 0, 9},
   {"bgezal", mips64_exec_BGEZAL, 0xfc1f0000, 0x04110000, 0, 9},
   {"bgezall", mips64_exec_BGEZALL, 0xfc1f0000, 0x04130000, 0, 9},
   {"bgezl", mips64_exec_BGEZL, 0xfc1f0000, 0x04030000, 0, 9},
   {"bgtz", mips64_exec_BGTZ, 0xfc1f0000, 0x1c000000, 0, 9},
   {"bgtzl", mips64_exec_BGTZL, 0xfc1f0000, 0x5c000000, 0, 9},
   {"blez", mips64_exec_BLEZ, 0xfc1f0000, 0x18000000, 0, 9},
   {"blezl", mips64_exec_BLEZL, 0xfc1f0000, 0x58000000, 0, 9},
   {"bltz", mips64_exec_BLTZ, 0xfc1f0000, 0x04000000, 0, 9},
   {"bltzal", mips64_exec_BLTZAL, 0xfc1f0000, 0x04100000, 0, 9},
   {"bltzall", mips64_exec_BLTZALL, 0xfc1f0000, 0x04120000, 0, 9},
   {"bltzl", mips64_exec_BLTZL, 0xfc1f0000, 0x04020000, 0, 9},
   {"bne", mips64_exec_BNE, 0xfc000000, 0x14000000, 0, 8},
   {"bnel", mips64_exec_BNEL, 0xfc000000, 0x54000000, 0, 8},
   {"break", mips64_exec_BREAK, 0xfc00003f, 0x0000000d, 1, 0},
   {"cache", mips64_exec_CACHE, 0xfc000000, 0xbc000000, 1, 2},
   {"clz", mips64_exec_CLZ, 0xfc0007FF, 0x70000020, 1, 2},
   {"div", mips64_exec_DIV, 0xfc00003f, 0x0000001a, 1, 17},
   {"divu", mips64_exec_DIVU, 0xfc00003f, 0x0000001b, 1, 17},
   {"eret", mips64_exec_ERET, 0xffffffff, 0x42000018, 0, 1},
   {"j", mips64_exec_J, 0xfc000000, 0x08000000, 0, 11},
   {"jal", mips64_exec_JAL, 0xfc000000, 0x0c000000, 0, 11},
   {"jalr", mips64_exec_JALR, 0xfc1f003f, 0x00000009, 0, 15},
   {"jr", mips64_exec_JR, 0xfc1ff83f, 0x00000008, 0, 13},
   {"lb", mips64_exec_LB, 0xfc000000, 0x80000000, 1, 2},
   {"lbu", mips64_exec_LBU, 0xfc000000, 0x90000000, 1, 2},
   {"lh", mips64_exec_LH, 0xfc000000, 0x84000000, 1, 2},
   {"lhu", mips64_exec_LHU, 0xfc000000, 0x94000000, 1, 2},
   {"ll", mips64_exec_LL, 0xfc000000, 0xc0000000, 1, 2},
   {"lui", mips64_exec_LUI, 0xffe00000, 0x3c000000, 1, 16},
   {"lw", mips64_exec_LW, 0xfc000000, 0x8c000000, 1, 2},
   {"lwl", mips64_exec_LWL, 0xfc000000, 0x88000000, 1, 2},
   {"lwr", mips64_exec_LWR, 0xfc000000, 0x98000000, 1, 2},
   {"lwu", mips64_exec_LWU, 0xfc000000, 0x9c000000, 1, 2},
   {"mad", mips64_exec_MAD, 0xfc00ffff, 0x70000000, 1, 18},
   {"madu", mips64_exec_MADU, 0xfc00ffff, 0x70000001, 1, 18},
   {"mfc0", mips64_exec_MFC0, 0xffe007f8, 0x40000000, 1, 18},
   {"mfhi", mips64_exec_MFHI, 0xffff07ff, 0x00000010, 1, 14},
   {"mflo", mips64_exec_MFLO, 0xffff07ff, 0x00000012, 1, 14},
   {"move", mips64_exec_MOVE, 0xfc1f07ff, 0x00000021, 1, 15},
   {"moven", mips64_exec_MOVEN, 0xfc0007ff, 0x0000000b, 1, 15},
   {"movez", mips64_exec_MOVEZ, 0xfc0007ff, 0x0000000a, 1, 15},
   {"msub", mips64_exec_MSUB, 0xfc00ffff, 0x70000004, 1, 18},
   {"msubu", mips64_exec_MSUBU, 0xfc00ffff, 0x70000005, 1, 18},
   {"mtc0", mips64_exec_MTC0, 0xffe007f8, 0x40800000, 1, 18},
   {"mthi", mips64_exec_MTHI, 0xfc1fffff, 0x00000011, 1, 13},
   {"mtlo", mips64_exec_MTLO, 0xfc1fffff, 0x00000013, 1, 13},
   {"mul", mips64_exec_MUL, 0xfc0007ff, 0x70000002, 1, 4},
   {"mult", mips64_exec_MULT, 0xfc00ffff, 0x00000018, 1, 17},
   {"multu", mips64_exec_MULTU, 0xfc00ffff, 0x00000019, 1, 17},
   {"nop", mips64_exec_NOP, 0xffffffff, 0x00000000, 1, 1},
   {"nor", mips64_exec_NOR, 0xfc0007ff, 0x00000027, 1, 3},
   {"or", mips64_exec_OR, 0xfc0007ff, 0x00000025, 1, 3},
   {"ori", mips64_exec_ORI, 0xfc000000, 0x34000000, 1, 5},
   {"pref", mips64_exec_PREF, 0xfc000000, 0xcc000000, 1, 0},
   {"prefi", mips64_exec_PREFI, 0xfc0007ff, 0x4c00000f, 1, 0},
   {"sb", mips64_exec_SB, 0xfc000000, 0xa0000000, 1, 2},
   {"sc", mips64_exec_SC, 0xfc000000, 0xe0000000, 1, 2},
   {"sh", mips64_exec_SH, 0xfc000000, 0xa4000000, 1, 2},
   {"sll", mips64_exec_SLL, 0xffe0003f, 0x00000000, 1, 7},
   {"sllv", mips64_exec_SLLV, 0xfc0007ff, 0x00000004, 1, 4},
   {"slt", mips64_exec_SLT, 0xfc0007ff, 0x0000002a, 1, 3},
   {"slti", mips64_exec_SLTI, 0xfc000000, 0x28000000, 1, 5},
   {"sltiu", mips64_exec_SLTIU, 0xfc000000, 0x2c000000, 1, 5},
   {"sltu", mips64_exec_SLTU, 0xfc0007ff, 0x0000002b, 1, 3},
   {"sra", mips64_exec_SRA, 0xffe0003f, 0x00000003, 1, 7},
   {"srav", mips64_exec_SRAV, 0xfc0007ff, 0x00000007, 1, 4},
   {"srl", mips64_exec_SRL, 0xffe0003f, 0x00000002, 1, 7},
   {"srlv", mips64_exec_SRLV, 0xfc0007ff, 0x00000006, 1, 4},
   {"sub", mips64_exec_SUB, 0xfc0007ff, 0x00000022, 1, 3},
   {"subu", mips64_exec_SUBU, 0xfc0007ff, 0x00000023, 1, 3},
   {"sw", mips64_exec_SW, 0xfc000000, 0xac000000, 1, 2},
   {"swl", mips64_exec_SWL, 0xfc000000, 0xa8000000, 1, 2},
   {"swr", mips64_exec_SWR, 0xfc000000, 0xb8000000, 1, 2},
   {"sync", mips64_exec_SYNC, 0xfffff83f, 0x0000000f, 1, 1},
   {"syscall", mips64_exec_SYSCALL, 0xfc00003f, 0x0000000c, 1, 1},
   {"teq", mips64_exec_TEQ, 0xfc00003f, 0x00000034, 1, 17},
   {"teqi", mips64_exec_TEQI, 0xfc1f0000, 0x040c0000, 1, 20},
   {"tlbp", mips64_exec_TLBP, 0xffffffff, 0x42000008, 1, 1},
   {"tlbr", mips64_exec_TLBR, 0xffffffff, 0x42000001, 1, 1},
   {"tlbwi", mips64_exec_TLBWI, 0xffffffff, 0x42000002, 1, 1},
   {"tlbwr", mips64_exec_TLBWR, 0xffffffff, 0x42000006, 1, 1},
   {"tne", mips64_exec_TNE, 0xfc00003f, 0x00000036, 1, 1},
   {"wait", mips64_exec_WAIT, 0xfc00003f, 0x40000020, 1, 3},
   {"xor", mips64_exec_XOR, 0xfc0007ff, 0x00000026, 1, 3},
   {"xori", mips64_exec_XORI, 0xfc000000, 0x38000000, 1, 5},
#if MIPS_64
   {"daddiu", mips64_exec_DADDIU, 0xfc000000, 0x64000000, 1, 5},
   {"daddu", mips64_exec_DADDU, 0xfc0007ff, 0x0000002d, 1, 3},
   {"dsll", mips64_exec_DSLL, 0xffe0003f, 0x00000038, 1, 7},
   {"dsll32", mips64_exec_DSLL32, 0xffe0003f, 0x0000003c, 1, 7},
   {"dsllv", mips64_exec_DSLLV, 0xfc0007ff, 0x00000014, 1, 4},
   {"dsra", mips64_exec_DSRA, 0xffe0003f, 0x0000003b, 1, 7},
   {"dsra32", mips64_exec_DSRA32, 0xffe0003f, 0x0000003f, 1, 7},
   {"dsrav", mips64_exec_DSRAV, 0xfc0007ff, 0x00000017, 1, 4},
   {"dsrl", mips64_exec_DSRL, 0xffe0003f, 0x0000003a, 1, 7},
   {"dsrl32", mips64_exec_DSRL32, 0xffe0003f, 0x0000003e, 1, 7},
   {"dsrlv", mips64_exec_DSRLV, 0xfc0007ff, 0x00000016, 1, 4},
   {"dsubu", mips64_exec_DSUBU, 0xfc0007ff, 0x0000002f, 1, 3},
   {"ld", mips64_exec_LD, 0xfc000000, 0xdc000000, 1, 2},
   {"ldl", mips64_exec_LDL, 0xfc000000, 0x68000000, 1, 2},
   {"ldr", mips64_exec_LDR, 0xfc000000, 0x6c000000, 1, 2},
   {"sd", mips64_exec_SD, 0xfc000000, 0xfc000000, 1, 2},
   {"sdl", mips64_exec_SDL, 0xfc000000, 0xb0000000, 1, 2},
   {"sdr", mips64_exec_SDR, 0xfc000000, 0xb4000000, 1, 2},
#endif
#if SOFT_FPU
   {"fpu", mips64_exec_SOFTFPU, 0xfc000000, 0x44000000, 0, 1},
   {"ldc1", mips64_exec_SOFTFPU, 0xfc000000, 0xd4000000, 1, 3},
   {"l.s", mips64_exec_SOFTFPU, 0xfc000000, 0xc4000000, 1, 3},
   {"swc1", mips64_exec_SOFTFPU, 0xfc000000, 0xe4000000, 1, 3},
   {"fpu", mips64_exec_SOFTFPU, 0xfc000000, 0xf4000000, 1, 3},
#endif
   {"unknown", mips64_exec_unknown, 0x00000000, 0x00000000, 1, 0},
   {NULL, NULL, 0x00000000, 0x00000000, 1, 0},
};
