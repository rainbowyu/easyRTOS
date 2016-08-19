#ifndef __EASYRTOSSEM_H
#define __EASYRTOSSEM_H

/* 信号量类型 */
#define SEM_BINARY  0x01
#define SEM_MUTEX   0x02
#define SEM_COUNTY  0x03

typedef struct easyRTOSSem
{
    EASYRTOS_TCB * suspQ;  /* 被Sem阻塞的任务队列 */
    EASYRTOS_TCB * owner;  /* 被MUTEX锁住的任务 */
    int8_t         count;  /* 信号量计数 -127 - 127*/
    uint8_t        type;   /* 信号量类型 */
} EASYRTOS_SEM;

typedef struct easyRTOSSemTimer
{
    EASYRTOS_TCB *tcb_ptr;  /* 具有timeout的悬挂任务 */
    EASYRTOS_SEM *sem_ptr;  /* 阻塞任务的信号量 */
} SEM_TIMER;

/* 全局函数 */
extern EASYRTOS_SEM eSemCreateBinary ();
extern EASYRTOS_SEM eSemCreateCount  (uint8_t initial_count);
extern EASYRTOS_SEM eSemCreateMutex  ();
extern ERESULT eSemDelete (EASYRTOS_SEM *sem);
extern ERESULT eSemTake (EASYRTOS_SEM *sem, int32_t timeout);
extern ERESULT eSemGive (EASYRTOS_SEM *sem);
extern ERESULT eSemResetCount (EASYRTOS_SEM *sem, uint8_t count);

#endif
