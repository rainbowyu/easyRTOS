#ifndef __EASYRTOSKERNEL_H__
#define __EASYRTOSKERNEL_H__

#include "easyRTOS.h"
#define ERESULT int8_t
#define EASYRTOS_OK            ( 0) /* 成功 */
#define EASYRTOS_ERR_PARAM     (-1) /* 错误的参数 */
#define EASYRTOS_ERR_QUEUE     (-2) /* 将任务加入Ready队列失败 */
#define EASYRTOS_ERR_NOT_FOUND (-3) /* 没有找到注册的timer */
#define EASYRTOS_ERR_CONTEXT   (-4) /* 错误的上下文调用 */
#define EASYRTOS_ERR_TIMER     (-5) /* 注册定时器失败/取消定时器失败*/
#define EASYRTOS_ERR_DELETED   (-6) /* 在悬挂任务时被删除 */
#define EASYRTOS_TIMEOUT       (-7) /* 信号量timeout到期 */
#define EASYRTOS_ERR_OVF       (-8) /* 计数信号量count>127或者小于 -127 */
#define EASYRTOS_WOULDBLOCK    (-9) /* 本来会被悬挂但由于timeout为-1所以返回了 */
#define EASYRTOS_ERR_BIN_OVF   (-10)/* 二值信号量count已经为1 */
#define EASYRTOS_SEM_UINIT     (-11)/* 信号量没有被初始化 */
#define EASYRTOS_ERR_OWNERSHIP (-12)/* 尝试解锁互斥锁的任务不是互斥锁拥有者 */

/* 全局函数 */
extern ERESULT eTaskCreat(EASYRTOS_TCB *task_tcb, uint8_t priority, void (*entry_point)(uint32_t), uint32_t entryParam, void* taskStack, uint32_t stackSize,const char* taskName,uint32_t taskID);
extern void easyRTOSStart (void);
extern void easyRTOSSched (uint8_t timer_tick);
ERESULT easyRTOSInit (void *idle_task_stack, uint32_t idleTaskStackSize);
extern ERESULT tcbEnqueuePriority (EASYRTOS_TCB **tcb_queue_ptr, EASYRTOS_TCB *tcb_ptr);
extern EASYRTOS_TCB *eCurrentContext (void);
extern void eIntEnter (void);
extern void eIntExit (uint8_t timerTick);
extern EASYRTOS_TCB *tcb_dequeue_entry (EASYRTOS_TCB **tcb_queue_ptr, EASYRTOS_TCB *tcb_ptr);
extern EASYRTOS_TCB *tcb_dequeue_head (EASYRTOS_TCB **tcb_queue_ptr);
extern EASYRTOS_TCB *tcb_dequeue_priority (EASYRTOS_TCB **tcb_queue_ptr, uint8_t priority);
/* end */

/* 全局变量 */
extern uint8_t easyRTOSStarted;
extern EASYRTOS_TCB *tcb_readyQ;

#endif