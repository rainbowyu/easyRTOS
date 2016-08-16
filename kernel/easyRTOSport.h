#ifndef __EASYRTOSPORT__H__
#define __EASYRTOSPORT__H__

#include "intrinsics.h"
/* Definition of NULL is available from stddef.h on this platform */
#include "stddef.h"

#define CRITICAL_STORE      __istate_t _istate
#define CRITICAL_ENTER()    _istate = __get_interrupt_state(); __disable_interrupt()
#define CRITICAL_EXIT()     __set_interrupt_state(_istate)

/* Required number of system ticks per second (normally 100 for 10ms tick) */
#define SYSTEM_TICKS_HZ                 100

/* Delay N ms , min(N) = 1000/SYSTEM_TICKS_HZ INT*/
#define DELAY_MS(x) ((x)*(SYSTEM_TICKS_HZ)/(1000))

/* Delay N ms , min(N) = 1/SYSTEM_TICKS_HZ INT*/
#define DELAY_S(x) ((x)*(SYSTEM_TICKS_HZ))

/* Size of each stack entry / stack alignment size (8 bits on STM8) */
#define STACK_ALIGN_SIZE                sizeof(u8)

extern void archTaskContextInit (EASYRTOS_TCB *tcb_ptr, void *stack_top, void (*entry_point)(uint32_t), uint32_t entryParam);
extern void archInitSystemTickTimer ( void );
#endif
