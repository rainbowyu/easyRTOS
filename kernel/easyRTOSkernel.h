#ifndef __EASYRTOSKERNEL_H__
#define __EASYRTOSKERNEL_H__

#include "easyRTOS.h"
#define ERESULT int8_t
#define EASYRTOS_OK 0               /* Success 成功 */
#define EASYRTOS_ERR_PARAM     (-1) /* Bad parameters 错误的参数 */
#define EASYRTOS_ERR_QUEUE     (-2) /* Error putting the task on the ready queue 将任务加入运行队列失败 */
#define EASYRTOS_ERR_NOT_FOUND (-3) /* Timer registration was not found 没有找到注册的timer */
#define EASYRTOS_ERR_CONTEXT   (-4) /* Not called from task context 错误的上下文调用*/
#define EASYRTOS_ERR_TIMER     (-5) /* Timer registration didn't work timer注册没有成功 */
/* public function */
extern ERESULT eTaskCreat(EASYRTOS_TCB *task_tcb, uint8_t priority, void (*entry_point)(uint32_t), uint32_t entryParam, void* taskStack, uint32_t stackSize,const char* taskName,uint32_t taskID);
extern void easyRTOSStart (void);
extern void easyRTOSSched (uint8_t timer_tick);
ERESULT easyRTOSInit (void *idle_task_stack, uint32_t idleTaskStackSize);
extern ERESULT tcbEnqueuePriority (EASYRTOS_TCB **tcb_queue_ptr, EASYRTOS_TCB *tcb_ptr);
extern EASYRTOS_TCB *eCurrentContext (void);
extern EASYRTOS_TCB *tcb_dequeue_head (EASYRTOS_TCB **tcb_queue_ptr);
extern EASYRTOS_TCB *tcb_dequeue_entry (EASYRTOS_TCB **tcb_queue_ptr, EASYRTOS_TCB *tcb_ptr);
extern EASYRTOS_TCB *tcb_dequeue_priority (EASYRTOS_TCB **tcb_queue_ptr, uint8_t priority);
extern void eIntEnter (void);
extern void eIntExit (uint8_t timerTick);
/* end */

/* public data*/
extern uint8_t easyRTOSStarted;
extern EASYRTOS_TCB *tcb_readyQ;

#endif