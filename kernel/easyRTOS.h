#define POINTER       void *
#define TASKNAMELEN   10
#define TASK_RUN      0x01    /*运行*/
#define TASK_READY    0x02    /*就绪*/
#define TASK_PENDED   0x04    /*悬挂*/
#define TASK_DELAY    0x08    /*延迟*/
#define TASK_SUSPEND  0x10    /*挂起*/
typedef struct easyRTOS_tcb
{
    /*
     * Task's current stack pointer. When a task is scheduled
     * out the architecture port can save its stack pointer here.
     * 任务栈指针.当任务被调度器切换的时候,栈指针保存在这个变量中.
     */
    POINTER sp_save_ptr;

    /* Task priority (0-255)
     * 线程的优先级 (0-255)
     */
    uint8_t priority;

    /* Task entry point and parameter
     * 任务函数入口以及参数.
     */
    void (*entry_point)(uint32_t);
    uint32_t entryParam;

    /* Queue pointer
     * 任务队列指针链表
     */
    struct easyRTOS_tcb *prev_tcb;    /* Previous TCB in doubly-linked TCB list 指向前一个TCB的双向TCB链表指针*/
    struct easyRTOS_tcb *next_tcb;    /* Next TCB in doubly-linked list 指向后一个TCB的双向TCB链表指针*/

    /* state of the task:           任务状态:
     * SUSPEND                      挂起
     * DELAY                        延迟
     * READY                        就绪
     * RUN                          运行
     * PENDED                       悬挂
     */
    uint8_t state;
    uint8_t pendedWakeStatus;      /* Status returned to woken suspend calls */
    EASYRTOS_TIMER *pended_timo_cb;  /* Callback registered for suspension timeouts */
    EASYRTOS_TIMER *delay_timo_cb;   /* Callback registered for delay timeouts */

    /* Task ID
     * 任务ID
     */
    uint8_t taskID;

    /* Task Name
     * 任务名称
     */
    uint8_t taskName[TASKNAMELEN];

    /* Task Run time after created
     * it is used to compute the CPU utilization rate
     * 任务在创建之后的实际运行态的时间,计算CPU使用率时使用.
     */
    uint32_t taskRunTime;

} EASYRTOS_TCB;
