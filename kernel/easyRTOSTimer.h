#ifndef __EASYRTOSTIMER_H__
#define __EASYRTOSTIMER_H__
/* Delay 回调结构体 */
typedef struct delay_timer
{
  EASYRTOS_TCB *tcb_ptr;  /* 被Delay的任务列表 */
}DELAY_TIMER;
  
/* 全局函数 */
extern void eTimerTick (void);
extern ERESULT eTimerDelay (uint32_t ticks);
extern void eTimerCallbacks (void);
extern ERESULT eTimerRegister (EASYRTOS_TIMER *timer_ptr);
extern ERESULT eTimerCancel (EASYRTOS_TIMER *timer_ptr);
extern uint32_t eTimeGet(void);
extern void eTimeSet(uint32_t newTime);
#endif
