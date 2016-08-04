static void thread_shell (void)
{
    ATOM_TCB *curr_tcb;

    /* Get the TCB of the thread being started */
    curr_tcb = atomCurrentContext();

    /**
     * Enable interrupts - these will not be enabled when a thread
     * is first restored.
     * 使能中断.
     */
    rim();

    /**
     *  Call the task entry point
     *  任务函数入口
     */
    if (curr_tcb && curr_tcb->entry_point)
    {
        curr_tcb->entry_point(curr_tcb->entry_param);
    }
}

void archThreadContextInit (ATOM_TCB *tcb_ptr, void *stack_top, void (*entry_point)(uint32_t), uint32_t entry_param)
{
    uint8_t *stack_ptr;
    stack_ptr = (uint8_t *)stack_top;
    /**
     *  The task restore routines will perform a RET which expects to
     *  find the address of the calling routine on the stack. In this case
     *  (the first time a task is run) we "return" to the entry point for
     *  the thread. That is, we store the task entry point in the
     *  place that RET will look for the return address: the stack.
     *  任务恢复程序会执行 RET 指令.该指令需要在堆栈中找到被切换的任务的程序地址.
     *  当一个任务第一次运行时,会返回任务入口.我们将任务入口存储在堆栈中,同时RET
     *  也将在堆栈中寻找任务程序返回地址.

     *  Note that we are using the task_shell() routine to start all
     *  tasks, so we actually store the address of thread_shell()
     *  here.
     *  我们用 task_shell() 函数启动所有的任务,所以实际上我们将thread_shell()的
     *  地址存储在这里.

     *  Because we are filling the stack from top to bottom, this goes
     *  on the stack first (at the top).
     *  从上到下使用堆栈.
     */
    *stack_ptr-- = (uint8_t)((uint16_t)thread_shell & 0xFF);
    *stack_ptr-- = (uint8_t)(((uint16_t)thread_shell >> 8) & 0xFF);

    /**
     * (IAR) Set up initial values for ?b8 to ?b15.
     * 使用IAR的时候会有一些虚拟寄存器,初始化这些虚拟寄存器的位置.
     */
    *stack_ptr-- = 0;    // ?b8
    *stack_ptr-- = 0;    // ?b9
    *stack_ptr-- = 0;    // ?b10
    *stack_ptr-- = 0;    // ?b11
    *stack_ptr-- = 0;    // ?b12
    *stack_ptr-- = 0;    // ?b13
    *stack_ptr-- = 0;    // ?b14
    *stack_ptr-- = 0;    // ?b15

    /**
     *  All task context has now been initialised. All that is left
     *  is to save the current stack pointer to the thread's TCB so
     *  that it knows where to start looking when the thread is started.
     *  与任务上下文有关的数据都已初始化完毕.剩下的就是在TCB中保存栈指针,供调度器
     *  使用.
     */
    tcb_ptr->sp_save_ptr = stack_ptr;
}


void archInitSystemTickTimer ( void )
{
    /* Reset TIM3 */
    TIM3_DeInit();

    /* Configure a 10ms tick */
    TIM3_TimeBaseInit(10000, TIM3_COUNTERMODE_UP, 16, 0);

    /* Generate an interrupt on timer count overflow */
    TIM3_ITConfig(TIM3_IT_UPDATE, ENABLE);

    /* Enable TIM3 */
    TIM3_Cmd(ENABLE);
}

/**
 *  System tick ISR.
 *  @return None
 */
#pragma vector = 13
INTERRUPT void TIM3_SystemTickISR (void)
{
    /* Call the OS system tick handler */
    easyRTOSTimerTick();

    /* Ack the interrupt (Clear TIM3:SR1 register bit 0) */
    TIM3->SR1 = (uint8_t)(~(uint8_t)TIM3_IT_UPDATE);
}
