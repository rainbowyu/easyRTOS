#ifndef __EASYRTOS__H__
#define __EASYRTOS__H__

#include "stm8s.h"
/* Constants */
#define TRUE                    1
#define FALSE                   0
/* End */

#define POINTER       void *
#define TASKNAMELEN   10
#define TASKSTATE     uint8_t

#define TASK_RUN      0x01    /*运行*/
#define TASK_READY    0x02    /*就绪*/
#define TASK_PENDED   0x04    /*悬挂*/
#define TASK_DELAY    0x08    /*延迟*/
#define TASK_SUSPEND  0x10    /*挂起*/

typedef void ( * TIMER_CB_FUNC ) ( POINTER cb_data ) ;

typedef struct easyRTOS_timer
{
  TIMER_CB_FUNC   cb_func;    /* 回调函数 */
  POINTER	        cb_data;    /* 回调函数的参数指针 */
  uint32_t	      cb_ticks;   /* 定时器count计数 */

	/* 内部数据 */
  struct easyRTOS_timer *next_timer;		/* 双向链表 */
} EASYRTOS_TIMER;

typedef struct easyRTOS_tcb
{
    /* 任务栈指针.当任务被调度器切换的时候,栈指针保存在这个变量中 */
    POINTER sp_save_ptr;

    /* 线程的优先级 (0-255) */
    uint8_t priority;

    /**
     *  任务函数入口以及参数.
     */
    void (*entry_point)(uint32_t);
    uint32_t entryParam;

    /* 任务队列指针链表 */
    struct easyRTOS_tcb *prev_tcb;    /* 指向前一个TCB的双向TCB链表指针*/
    struct easyRTOS_tcb *next_tcb;    /* 指向后一个TCB的双向TCB链表指针*/

    /**
     *  任务状态:
     *  挂起
     *  延迟
     *  就绪
     *  运行
     *  悬挂
     */
    TASKSTATE state;
    int8_t pendedWakeStatus;        
    EASYRTOS_TIMER *pended_timo_cb;  
    EASYRTOS_TIMER *delay_timo_cb;   

    /* 任务ID */
    uint8_t taskID;

    /* 任务名称 */
    uint8_t taskName[TASKNAMELEN];

    /* 任务在创建之后的实际运行态的时间,计算CPU使用率时使用 */
    uint32_t taskRunTime;
} EASYRTOS_TCB;

extern void archContextSwitch (EASYRTOS_TCB *old_tcb_ptr, EASYRTOS_TCB *new_tcb_ptr);
extern void archFirstTaskRestore (EASYRTOS_TCB *new_tcb_ptr);
#endif
