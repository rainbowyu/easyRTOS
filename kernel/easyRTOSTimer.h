#ifndef __EASYRTOSTIMER_H__
#define __EASYRTOSTIMER_H__
/* Delay callbacks data structure */
typedef struct delay_timer
{
  EASYRTOS_TCB *tcb_ptr;  /* Task which is DELAY with timeout */
}DELAY_TIMER;
  
extern void eTimerTick (void);
extern ERESULT eTimerDelay (uint32_t ticks);
#endif
