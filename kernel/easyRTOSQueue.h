#ifndef __EASYRTOSQUEUE_H
#define __EASYRTOSQUEUE_H

typedef struct eQueue
{
    EASYRTOS_TCB *  putSuspQ;   /* 等待任务数据发送的任务队列 */
    EASYRTOS_TCB *  getSuspQ;   /* 等待任务数据接收的任务队列 */
    void *   buff_ptr;          /* 队列数据指针 */
    uint32_t    unit_size;      /* 单个消息的大小 */
    uint32_t    max_num_msgs;   /* 总消息数量 */
    uint32_t    insert_index;   /* 消息插入索引 */
    uint32_t    remove_index;   /* 消息移除索引 */
    uint32_t    num_msgs_stored;/* 现有消息数量 */
} EASYRTOS_QUEUE;

typedef struct eQueuetimer
{
    EASYRTOS_TCB   *tcb_ptr;    /* Thread which is suspended with timeout */
    EASYRTOS_QUEUE *queue_ptr;  /* Queue the thread is interested in */
    EASYRTOS_TCB   **suspQ;     /* TCB queue which thread is suspended on */
} QUEUE_TIMER;

/* 全局函数 */
extern EASYRTOS_QUEUE eQueueCreate ( void *buff_ptr, uint32_t unit_size, uint32_t max_num_msgs);
extern ERESULT eQueueDelete (EASYRTOS_QUEUE *qptr);
extern ERESULT eQueueTake (EASYRTOS_QUEUE *qptr, int32_t timeout, void *msgptr);
extern ERESULT eQueueGive (EASYRTOS_QUEUE *qptr, int32_t timeout, void *msgptr);

#endif
