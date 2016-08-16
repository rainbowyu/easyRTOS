#include "easyRTOS.h"
#include "easyRTOSkernel.h"
#include "easyRTOSport.h"

/**
 *  running task's TCB
 *  正在运行的任务的TCB
 */
static EASYRTOS_TCB *curr_tcb = NULL;
/**
 *  easyRTOS started flag
 *  easyRTOS启动标志位
 */
uint8_t easyRTOSStarted = FALSE;

/**
 *  easyRTOS task ready queue
 *  easyRTOS就绪任务队列
 */
EASYRTOS_TCB *tcb_readyQ = NULL;

/**
 *  number of easyRTOS IT counter
 *  easyRTOS中断嵌套计数
 */
static int easyITCnt = 0;

static EASYRTOS_TCB idleTcb;

/* public function */
ERESULT eTaskCreat(EASYRTOS_TCB *tcb_ptr, uint8_t priority, void (*entry_point)(uint32_t), uint32_t entryParam, void* task_stack, uint32_t stackSize,const char* task_name,uint32_t taskID);
void easyRTOSStart (void);
void easyRTOSSched (uint8_t timer_tick);
ERESULT easyRTOSInit (void *idle_task_stack, uint32_t idleTaskStackSize);
ERESULT tcbEnqueuePriority (EASYRTOS_TCB **tcb_queue_ptr, EASYRTOS_TCB *tcb_ptr);
EASYRTOS_TCB *eCurrentContext (void);
EASYRTOS_TCB *tcb_dequeue_head (EASYRTOS_TCB **tcb_queue_ptr);
EASYRTOS_TCB *tcb_dequeue_entry (EASYRTOS_TCB **tcb_queue_ptr, EASYRTOS_TCB *tcb_ptr);
EASYRTOS_TCB *tcb_dequeue_priority (EASYRTOS_TCB **tcb_queue_ptr, uint8_t priority);
void eIntEnter (void);
void eIntExit (uint8_t timerTick);
/* end */

/* private function */
static void idleTask (uint32_t param);
static void eTaskSwitch(EASYRTOS_TCB *old_tcb, EASYRTOS_TCB *new_tcb);
/* end */

ERESULT eTaskCreat(EASYRTOS_TCB *tcb_ptr, uint8_t priority, void (*entry_point)(uint32_t), uint32_t entryParam, void* task_stack, uint32_t stackSize,const char* task_name,uint32_t taskID)
{
  ERESULT status;
  CRITICAL_STORE;
  uint8_t i = 0;
  char *p_string = NULL;
  uint8_t *stack_top = NULL;

  if ((tcb_ptr == NULL) || (entry_point == NULL) || (task_stack == NULL) || (stackSize == 0) || task_name == NULL)
  {
    /**
     *  参数错误
     *  Bad parameters
     */
    status = EASYRTOS_ERR_PARAM;
  }
  else
  {
    /**
     *  初始化TCB
     *  Set up the TCB initial values   
     */
    for (p_string=(char *)task_name;(*p_string)!='\0';p_string++)
    {
      tcb_ptr->taskName[i]=*p_string;
      i++;
      if (i > TASKNAMELEN)
      {
        status = EASYRTOS_ERR_PARAM;
      }
    }
    tcb_ptr->taskID = taskID;
    tcb_ptr->state = TASK_READY;
    tcb_ptr->priority = priority;
    tcb_ptr->prev_tcb = NULL;
    tcb_ptr->next_tcb = NULL;
    tcb_ptr->pended_timo_cb = NULL;
    tcb_ptr->delay_timo_cb = NULL;

    /**
     * 在TCB中保存任务函数入口以及参数.
     * Store the task entry point and parameter in the TCB.
     */
    tcb_ptr->entry_point = entry_point;
    tcb_ptr->entryParam  = entryParam;;

    /**
     * 计算栈顶入口地址.
     * Calculate a pointer to the topmost stack entry.
     */
    stack_top = (uint8_t *)task_stack + (stackSize & ~(STACK_ALIGN_SIZE - 1)) - STACK_ALIGN_SIZE;

    /**
     * 调用arch-specific函数去初始化堆栈,这个函数负责建立一个任务上下文存储区域,
     * 以便easyRTOSTaskSwitch()对该任务进行调度.archContextSwitch() (在任务被
     * 调度的一开始就会被调用)函数会从任务入口恢复程序计数以及其他的一些必须用到寄
     * 存器值,以便以任务继续执行.
    
     * Call the arch-specific routine to set up the stack. This routine
     * is responsible for creating the context save area necessary for
     * allowing easyRTOSTaskSwitch() to schedule it in. The initial
     * archContextSwitch() call when this task gets scheduled in the
     * first time will then restore the program counter to the task
     * entry point, and any other necessary register values ready for
     * it to start running.
     */
    archTaskContextInit (tcb_ptr, stack_top, entry_point, entryParam);

    /**
     *  进入临界区,保护系统任务队列.
     *  Protect access to the OS queue
     */
    CRITICAL_ENTER ();

    /**
     *  将新建的任务加入就绪队列
     *  Put this task on the ready queue
     */
    if (tcbEnqueuePriority (&tcb_readyQ, tcb_ptr) != EASYRTOS_OK)
    {
      /**
       *  若加入失败则退出临界区
       *  Exit critical region
       */
      CRITICAL_EXIT ();

      /**
       *  返回与就绪队列相关的错误
       *  Queue-related error
       */
      status = EASYRTOS_ERR_QUEUE;
    }
    else
    {
      /**
       *  正常退出临界区
       *  Exit critical region
       */
      CRITICAL_EXIT ();

      /**
       *  若系统已经启动,已经进入任务.则立刻启动调度器.
       *  If the OS is started and we're in task context, check if we
       *  should be scheduled in now.
       */
      if (easyRTOSStarted == TRUE)
          easyRTOSSched (FALSE);

      /**
       *  返回任务创建成功标志
       *  Success
       */
      status = EASYRTOS_OK;
    }
  }
  return (status);
}

ERESULT easyRTOSInit (void *idle_task_stack, uint32_t idleTaskStackSize)
{
    uint8_t status;

    /**
     *  初始化数据
     *  Initialise data 
     */
    curr_tcb = NULL;
    tcb_readyQ = NULL;
    easyRTOSStarted = FALSE;

    /** 
     *  创建空任务
     *  Create the idle task
     */
    status = eTaskCreat(&idleTcb,
                 255,
                 idleTask,
                 0,
                 idle_task_stack,
                 idleTaskStackSize,
                 "IDLE",
                 0);
    
    /* Return status */
    return (status);
}

void easyRTOSStart (void)
{
    EASYRTOS_TCB *new_tcb;

    /**
     *  置位系统启动标志位,这个标志位会影响eTaskCreate(),使得该函数不会启动调度器.
     *  Enable the OS started flag. This stops routines like eTaskCreate()
     *  attempting to schedule in a newly-created task until the scheduler is
     *  up and running.
     */
    easyRTOSStarted = TRUE;

    /**
     *  取出优先级最高的TCB,并启动调度器.若没有创建任何任务则会启动优先级最低的空任务
     *  Take the highest priority task and schedule it in. If no any tasks
     *  were created, the OS will simply start the idle task (the lowest
     *  priority allowed to be scheduled is the idle task's priority, 255).
     */
    new_tcb = tcb_dequeue_priority (&tcb_readyQ, 255);
    if (new_tcb)
    {
      curr_tcb = new_tcb;

      /**
       *  恢复并运行第一个任务
       *  Restore and run the first task
       */
      archFirstTaskRestore (new_tcb);

    }
    else
    {
      /**
       *  没找到可以运行的任务,
       *  No ready tasks were found. easyRTOSInit() probably was not called
       */
    }
}

void easyRTOSSched (uint8_t timer_tick)
{
    CRITICAL_STORE;
    EASYRTOS_TCB *new_tcb = NULL;
    int16_t lowest_pri;

    /**
     * 检测RTOS是否开启.
     * Check the easyRTOS has actually started.
     */
    if (easyRTOSStarted == FALSE)
    {
      return;
    }

    /**
     *  进入临界区.
     *  Enter critical section
     */
    CRITICAL_ENTER();

    if (curr_tcb->state != TASK_RUN)
    {
      /**
       *  从已经就绪的任务队列中取出一个.队列中必然有idleTask
       *  Dequeue the next ready to run task. There will always be
       *  at least the idle task waiting.
       */
      new_tcb = tcb_dequeue_head (&tcb_readyQ);

      /**
       *  切换成新任务.
       *  Switch to the new task
       */
      eTaskSwitch (curr_tcb, new_tcb);
    }

    /**
     * 若任务仍然为运行态,则检查是否有其他任务已经就绪,并需要运行.
     * Otherwise the current task is still ready, but check
     * if any other tasks are ready.
     */
    else
    {
      /**
       *  计算允许调度的优先级
       *  Calculate which priority is allowed to be scheduled in
       */
      if (timer_tick == TRUE)
      {
        /**
         *  相同或者更高优先级的任务可以抢占.
         *  Same priority or higher tasks can preempt 
         */
        lowest_pri = (int16_t)curr_tcb->priority;
      }
      else if (curr_tcb->priority > 0)
      {
        /**
         *  只有更高的优先级才能抢占CPU,最高优先级为0
         *  Only higher priority tasks can preempt, invalid for 0 (highest)
         */
        lowest_pri = (int16_t)(curr_tcb->priority - 1);
      }
      else
      {
        /**
         * 目前的优先级已经最高了,不允许任何线程抢占.
         * Current priority is already highest (0), don't allow preempt by
         * tasks of any priority .
         */
        lowest_pri = -1;
      }

      /**
       *  检查是否进行调度.
       *  Check if a reschedule is allowed
       */
      if (lowest_pri >= 0)
      {
        /**
         *  检查是否有不低于给出优先级的任务.
         *  Check for a task at the given minimum priority level or higher
         */
        new_tcb = tcb_dequeue_priority (&tcb_readyQ, (uint8_t)lowest_pri);

        /**
         *  若找到对应任务,运行之.
         *  If a task was found, schedule it in
         */
        if (new_tcb)
        {
          /**
           *  将现在运行的任务加入就绪队列.
           *  Add the current task to the ready queue
           */
          (void)tcbEnqueuePriority (&tcb_readyQ, curr_tcb);

          /**
           *  切换到新任务.
           *  Switch to the new task
           */
          eTaskSwitch (curr_tcb, new_tcb);
        }
      }
    }

    /**
     *  退出临界区
     *  Exit critical section
     */
    CRITICAL_EXIT ();
}

ERESULT tcbEnqueuePriority (EASYRTOS_TCB **tcb_queue_ptr, EASYRTOS_TCB *tcb_ptr)
{
    ERESULT status;
    EASYRTOS_TCB *prev_ptr, *next_ptr;

    /**
     *  参数检查
     *  Parameter check
     */
    if ((tcb_queue_ptr == NULL) || (tcb_ptr == NULL))
    {
      /**
       *  返回错误
       *  Return error
       */
      status = EASYRTOS_ERR_PARAM;
    }
    else
    {
      /**
       *  搜索整个队列
       *  Walk the list and enqueue at the end of the TCBs at this priority
       */
      prev_ptr = next_ptr = *tcb_queue_ptr;
      do
      {
        /** 
         *  插入条件:
         *   next_ptr = NULL (已经到队列的末尾了.)
         *   下一个TCB的优先级比要插入的TCB优先级低.
         *  Insert if:
         *   next_ptr = NULL (we're at the head of an empty queue or at the tail)
         *   the next TCB in the list is lower priority than the one we're enqueuing.
         */
        if ((next_ptr == NULL) || (next_ptr->priority > tcb_ptr->priority))
        {
          /**
           *  若优先级为队列最高,则将该TCB作为队列的头
           *  Make this TCB the new queuehead 
           */
          if (next_ptr == *tcb_queue_ptr)
          {
            *tcb_queue_ptr = tcb_ptr;
            tcb_ptr->prev_tcb = NULL;
            tcb_ptr->next_tcb = next_ptr;
            if (next_ptr)
                next_ptr->prev_tcb = tcb_ptr;
          }
          /**
           *  在队列中插入TCB或者在末尾插入
           *  Insert between two TCBs or at the tail
           */
          else
          {
            tcb_ptr->prev_tcb = prev_ptr;
            tcb_ptr->next_tcb = next_ptr;
            prev_ptr->next_tcb = tcb_ptr;
            if (next_ptr)
                next_ptr->prev_tcb = tcb_ptr;
          }

          /**
           *  完成插入,退出循环
           *  Quit the loop, we've finished inserting
           */
          break;
        }
        else
        {
          /**
           *  插入位置不对,尝试下一个位置
           *  Not inserting here, try the next one
           */
          prev_ptr = next_ptr;
          next_ptr = next_ptr->next_tcb;
        }

      }
      while (prev_ptr != NULL);

      /* 成功 Successful */
      status = EASYRTOS_OK;
    }
    return (status);
}

EASYRTOS_TCB *eCurrentContext (void)
{
    if (easyITCnt == 0)
        return (curr_tcb);
    else
        return (NULL);
}

EASYRTOS_TCB *tcb_dequeue_head (EASYRTOS_TCB **tcb_queue_ptr)
{
    EASYRTOS_TCB *ret_ptr;

    /**
     *  参数检查
     *  Parameter check
     */
    if (tcb_queue_ptr == NULL)
    {
        /**
         *  返回NULL
         *  Return NULL    
         */
        ret_ptr = NULL;
    }
    else if (*tcb_queue_ptr == NULL)
    {
        /**
         *  返回NULL
         *  Return NULL
         */
        ret_ptr = NULL;
    }
    /**
     *  返回队列头,并将其移除队列
     *  Remove and return the queuehead
     */
    else
    {
        ret_ptr = *tcb_queue_ptr;
        *tcb_queue_ptr = ret_ptr->next_tcb;
        if (*tcb_queue_ptr)
            (*tcb_queue_ptr)->prev_tcb = NULL;
        ret_ptr->next_tcb = ret_ptr->prev_tcb = NULL;
    }
    return (ret_ptr);
}

EASYRTOS_TCB *tcb_dequeue_entry (EASYRTOS_TCB **tcb_queue_ptr, EASYRTOS_TCB *tcb_ptr)
{
    EASYRTOS_TCB *ret_ptr, *prev_ptr, *next_ptr;

    /**
     *  参数检查
     *  Parameter check
     */
    if (tcb_queue_ptr == NULL)
    {
        /**
         *  返回NULL
         *  Return NULL     
         */
        ret_ptr = NULL;
    }
    else if (*tcb_queue_ptr == NULL)
    {
        /**
         *  返回NULL
         *  Return NULL
         */
        ret_ptr = NULL;
    }
    /**
     *  找到,移除并返回指定的tcb
     *  Find and remove/return the specified entry  
     */
    else
    {
        ret_ptr = NULL;
        prev_ptr = next_ptr = *tcb_queue_ptr;
        while (next_ptr)
        {
            /**
             *  是否是我们搜索的TCB?
             *  Is this entry the one we're looking for?
             */
            if (next_ptr == tcb_ptr)
            {
                /**
                 *  寻找的TCB为队列头
                 *  entry is queuehead
                 */
                if (next_ptr == *tcb_queue_ptr)
                {
                    /**
                     *  移除队列头.
                     *  We're removing the queue head        
                     */
                    *tcb_queue_ptr = next_ptr->next_tcb;
                    if (*tcb_queue_ptr)
                        (*tcb_queue_ptr)->prev_tcb = NULL;
                }
                /**
                 *  寻找的TCB在队列中间或末尾.
                 *  entry is a mid or tail
                 */
                else
                {
                    prev_ptr->next_tcb = next_ptr->next_tcb;
                    if (next_ptr->next_tcb)
                        next_ptr->next_tcb->prev_tcb = prev_ptr;
                }
                ret_ptr = next_ptr;
                ret_ptr->prev_tcb = ret_ptr->next_tcb = NULL;
                break;
            }

            /**
             *  移动到队列中的下一个TCB
             *  Move on to the next in the queue 
             */
            prev_ptr = next_ptr;
            next_ptr = next_ptr->next_tcb;
        }
    }
    return (ret_ptr);
}

EASYRTOS_TCB *tcb_dequeue_priority (EASYRTOS_TCB **tcb_queue_ptr, uint8_t priority)
{
    EASYRTOS_TCB *ret_ptr;

    /**
     *  参数检查
     *  Parameter check
     */
    if (tcb_queue_ptr == NULL)
    {
      /* Return NULL */
      ret_ptr = NULL;
    }
    else if (*tcb_queue_ptr == NULL)
    {
      /* Return NULL */
      ret_ptr = NULL;
    }
    /**
     *  检查是否有合适优先级的任务
     *  Check if the list head priority is within our range    
     */
    else if ((*tcb_queue_ptr)->priority <= priority)
    {
      /* Remove the list head */
      ret_ptr = *tcb_queue_ptr;
      *tcb_queue_ptr = (*tcb_queue_ptr)->next_tcb;
      if (*tcb_queue_ptr)
      {
        (*tcb_queue_ptr)->prev_tcb = NULL;
        ret_ptr->next_tcb = NULL;
      }
    }
    else
    {
      /**
       *  没有合适任务
       *  No higher priority ready task found    
       */
      ret_ptr = NULL;
    }
    return (ret_ptr);
}



static void eTaskSwitch(EASYRTOS_TCB *old_tcb, EASYRTOS_TCB *new_tcb)
{
    /**
     *  运行的程序将切换到新的任务,所以将任务状态切换到RUN
     *  The context switch will shift execution to a different task. The
     *  new task is now ready to run so trun its status in TASK_RUN
     *  preparation for it waking up.
     */
    new_tcb->state = TASK_RUN;

    /**
     *  检查新的任务是否是目前运行的任务,如果是,则不需要进行切换.
     *  Check if the new task is actually the current one, in which
     *  case we don't need to do any context switch.
     */
    if (old_tcb != new_tcb)
    {
        curr_tcb = new_tcb;

        /**
         *  调用任务切换函数.
         *  Call the architecture-specific context switch
         */
        archContextSwitch (old_tcb, new_tcb);
    }
}

void eIntEnter (void)
{
    /** 
     *  增加中断计数
     *  Increment the interrupt count 
     */
    easyITCnt++;
}

void eIntExit (uint8_t timerTick)
{
    /** 
     *  退出中断时减少
     *  Decrement the interrupt count 
     */
    easyITCnt--;

    /**
     *  退出中断时调用调度器
     *  Call the scheduler 
     */
    easyRTOSSched (timerTick);
}

static void idleTask (uint32_t param)
{
  /**
   *  消除编译器警告
   *  Compiler warning
   */
  param = param;

  /* Loop forever */
  while (1)
  {
     /**
      * 空函数执行
      */
  }
}
