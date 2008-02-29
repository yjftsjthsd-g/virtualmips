#ifndef __JZ4740_H__
#define __JZ4740_H__



#include "types.h"

/*virtual address and physical address*/
typedef m_uint32_t m_va_t;
typedef m_uint32_t m_pa_t;
typedef m_uint32_t m_reg_t;
typedef m_int32_t m_ireg_t;
typedef m_uint32_t m_cp0_reg_t; 


#define  DATA_WIDTH 32 /*64*/
#define LL



/*Guest endian*/
#define GUEST_BYTE_ORDER  ARCH_LITTLE_ENDIAN
#ifndef GUEST_BYTE_ORDER
#error Please define guest architecture in utils.h!
#endif


/* Host to VM conversion functions */
#if HOST_BYTE_ORDER == GUEST_BYTE_ORDER
#define htovm16(x) (x)
#define htovm32(x) (x)
#define htovm64(x) (x)

#define vmtoh16(x) (x)
#define vmtoh32(x) (x)
#define vmtoh64(x) (x)
#elif HOST_BYTE_ORDER==ARCH_LITTLE_ENDIAN  //host:little guest:big
#define htovm16(x) (htons(x))
#define htovm32(x) (htonl(x))
#define htovm64(x) (swap64(x))

#define vmtoh16(x) (ntohs(x))
#define vmtoh32(x) (ntohl(x))
#define vmtoh64(x) (swap64(x))
#else   //host:big guest:little

#define htovm16(x) (ntohs(x))
#define htovm32(x) (ntohl(x))
#define htovm64(x) (swap64(x))

#define vmtoh16(x) (htons(x))
#define vmtoh32(x) (htonl(x))
#define vmtoh64(x) (swap64(x))
#endif







#endif



