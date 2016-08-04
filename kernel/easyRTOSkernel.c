/**
 *  running task's TCB
 *  正在运行的任务的TCB
 */
static easyRTOS_TCB *curr_tcb = NULL;
/**
 *  easyRTOS started flag
 *  easyRTOS启动标志位
 */
uint8_t easyRTOSStarted = FALSE;

ERESULT eTaskCreat(EASYRTOS_TCB *task_tcb, uint8_t priority, void (*entry_point)(uint32_t), uint32_t entryParam, void* taskStack, uint32_t stackSize,const char* taskName,uint32_t taskID)
{
  ERESULT status;
  uint8_t i = 0;
  char *p_string = NULL;
  uint8_t *stack_top = NULL;
  if ((tcbPtr == NULL) || (entryPoint == NULL) || (taskStack == NULL) || (stackSize == 0) || taskName == NULL)
  {
      /**
       *  Bad parameters
       *  参数错误
       */
      status = EASYRTOS_ERR_PARAM;
  }
  else
  {
    /**
     *  Set up the TCB initial values
     *  初始化TCB
     */
    for (p_string=(char *)taskName;(*p_string)!='\0';p_string++)
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
     * Store the task entry point and parameter in the TCB.
     * 在TCB中保存任务函数入口以及参数.
     */
    tcb_ptr->entry_point = entry_point;
    tcb_ptr->entryParam  = entryParam;;

    /**
     * Calculate a pointer to the topmost stack entry.
     * 计算栈顶入口地址.
     */
    stack_top = (uint8_t *)stack_bottom + stackSize;

    /**
     * Call the arch-specific routine to set up the stack. This routine
     * is responsible for creating the context save area necessary for
     * allowing easyRTOSTaskSwitch() to schedule it in. The initial
     * archContextSwitch() call when this task gets scheduled in the
     * first time will then restore the program counter to the task
     * entry point, and any other necessary register values ready for
     * it to start running.
     * 调用arch-specific函数去初始化堆栈,这个函数负责建立一个任务上下文存储区域,
     * 以便easyRTOSTaskSwitch()对该任务进行调度.archContextSwitch() (在任务被
     * 调度的一开始就会被调用)函数会从任务入口恢复程序计数以及其他的一些必须用到寄
     * 存器值,以便以任务继续执行.
     */
    archThreadContextInit (tcb_ptr, stack_top, entry_point, entry_param);

    /**
     *  Protect access to the OS queue
     *  进入临界区,保护系统任务队列.
     */
    CRITICAL_ENTER ();

    /**
     *  Put this task on the ready queue
     *  将新建的任务加入就绪队列
     */
    if (tcbEnqueuePriority (&tcbReadyQ, tcb_ptr) != ATOM_OK)
    {
      /**
       *  Exit critical region
       *  若加入失败则退出临界区
       */
      CRITICAL_EXIT ();

      /**
       *  Queue-related error
       *  返回与就绪队列相关的错误
       */
      status = EASYRTOS_ERR_QUEUE;
    }
    else
    {
      /**
       *  Exit critical region
       *  正常退出临界区
       */
      CRITICAL_EXIT ();

      /**
       *  If the OS is started and we're in task context, check if we
       *  should be scheduled in now.
       *  若系统已经启动,已经进入任务.则立刻启动调度器.
       */
      if ((atomOSStarted == TRUE) && atomCurrentContext())
          atomSched (FALSE);

      /**
        *  Success
        *  返回任务创建成功标志
        */
      status = EASYRTOS_OK;
    }
  }
  return (status);
}

void atomOSStart (void)
{
    ERESULT status;
    ATOM_TCB *new_tcb;

    /**
     *  Enable the OS started flag. This stops routines like eTaskCreate()
     *  attempting to schedule in a newly-created thread until the scheduler is
     *  up and running.
     *  置位系统启动标志位,这个标志位会影响eTaskCreate(),使得该函数不会启动调度器.
     */
    easyRTOSStarted = TRUE;

    /**
     *  Take the highest priority task and schedule it in. If no any tasks
     *  were created, the OS will simply start the idle task (the lowest
     *  priority allowed to be scheduled is the idle task's priority, 255).
     *  取出优先级最高的TCB,并启动调度器.若没有创建任何任务则会启动优先级最低的空任务
     */
    new_tcb = tcbDequeuePriority (&tcbReadyQ, 255);
    if (new_tcb)
    {
        curr_tcb = new_tcb;

        /**
         *  Restore and run the first task
         *  恢复并运行第一个任务
         */
        archFirstTaskRestore (new_tcb);

        /* Never returns to here, execution shifts to new thread context */
    }
    else
    {
        /* No ready threads were found. atomOSInit() probably was not called */
    }
}

void easyRTOSSched (uint8_t timer_tick)
{
    CRITICAL_STORE;
    ATOM_TCB *new_tcb = NULL;
    int16_t lowest_pri;

    /**
     * Check the easyRTOS has actually started.
     * 检测RTOS是否开启.
     */
    if (atomOSStarted == FALSE)
    {
        /* Don't schedule anything in until the OS is started */
        return;
    }

    /**
     *  Enter critical section
     *  进入临界区.
     */
    CRITICAL_START ();

    /**
     * If the current task is going into not ready or is being
     * terminated (run to completion), then unconditionally dequeue
     * the next thread for execution.
     */
    if (curr_tcb->state != RUN)
    {
        /**
         *  Dequeue the next ready to run task. There will always be
         *  at least the idle task waiting.
         *  从已经就绪的任务队列中取出一个.队列中必然有idleTask
         */
        new_tcb = tcbDequeueHead (&tcbReadyQ);

        /**
         *  Switch to the new task
         *  切换成新任务.
         */
        atomThreadSwitch (curr_tcb, new_tcb);
    }

    /**
     * Otherwise the current thread is still ready, but check
     * if any other threads are ready.
     * 若任务仍然为运行态,则检查是否有其他任务已经就绪,并需要运行.
     */
    else
    {
        /**
         *  Calculate which priority is allowed to be scheduled in
         *  计算允许调度的优先级
         */
        if (timer_tick == TRUE)
        {
            /* Same priority or higher threads can preempt */
            lowest_pri = (int16_t)curr_tcb->priority;
        }
        else if (curr_tcb->priority > 0)
        {
            /**
             *  Only higher priority threads can preempt, invalid for 0 (highest)
             *  只有更高的优先级才能抢占CPU,最高优先级为0
             */
            lowest_pri = (int16_t)(curr_tcb->priority - 1);
        }
        else
        {
            /**
             * Current priority is already highest (0), don't allow preempt by
             * threads of any priority .
             * 目前的优先级已经最高了,不允许任何线程抢占.
             */
            lowest_pri = -1;
        }

        /**
         *  Check if a reschedule is allowed
         *  检查是否进行调度
         */
        if (lowest_pri >= 0)
        {
            /* Check for a thread at the given minimum priority level or higher */
            new_tcb = tcbDequeuePriority (&tcbReadyQ, (uint8_t)lowest_pri);

            /* If a thread was found, schedule it in */
            if (new_tcb)
            {
                /* Add the current thread to the ready queue */
                (void)tcbEnqueuePriority (&tcbReadyQ, curr_tcb);

                /* Switch to the new thread */
                atomThreadSwitch (curr_tcb, new_tcb);
            }
        }
    }

    /* Exit critical section */
    CRITICAL_END ();
}

static void idleTask (uint32_t param)
{
    /**
     *  Compiler warning
     *  消除编译器警告
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
