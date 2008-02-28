 /*
 * Copyright (C) yajin 2008 <yajinzhou@gmail.com >
 *     
 * This file is part of the virtualmips distribution. 
 * See LICENSE file for terms of the license. 
 *
 */
 

#ifndef __TYPES_H__
#define __TYPES_H__


/* Common types */
typedef unsigned char m_uint8_t;
typedef signed char m_int8_t;

typedef unsigned short m_uint16_t;
typedef signed short m_int16_t;

typedef unsigned int m_uint32_t;
typedef signed int m_int32_t;

typedef unsigned long long m_uint64_t;
typedef signed long long m_int64_t;

typedef unsigned long m_iptr_t;
typedef m_uint64_t m_tmcnt_t;

/* MIPS instruction */
typedef m_uint32_t mips_insn_t;

/* True/False definitions */
#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE  1
#endif

//for gdb interface
/* Used for functions which can fail */
enum result_t {SUCCESS, FAILURE, STALL, BUSERROR, SCFAILURE};

/* Forward declarations */
typedef struct cpu_mips cpu_mips_t;
typedef struct vm_instance vm_instance_t;
typedef struct vdevice vdevice_t;

#endif

