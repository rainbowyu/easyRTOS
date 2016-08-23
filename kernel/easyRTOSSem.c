#include "easyRTOS.h"
#include "easyRTOSkernel.h"
#include "easyRTOSport.h"
#include "easyRTOSTimer.h"
#include "easyRTOSSem.h"

/* 全局函数 */
EASYRTOS_SEM eSemCreateCount (uint8_t initial_count);
EASYRTOS_SEM eSemCreateBinary (void);
EASYRTOS_SEM eSemCreateMutex (void);
ERESULT eSemDelete (EASYRTOS_SEM *sem);
ERESULT eSemTake (EASYRTOS_SEM *sem, int32_t timeout);
ERESULT eSemGive (EASYRTOS_SEM * sem);
ERESULT eSemResetCount (EASYRTOS_SEM *sem, uint8_t count);

/* 私有函数 */
static void eSemTimerCallback (POINTER cb_data);

EASYRTOS_SEM eSemCreateCount (uint8_t initial_count)
{
  EASYRTOS_SEM sem;

  /* 设置初始计数 */
  sem.count = initial_count;

  /* 初始化被信号量悬挂的任务的队列 */
  sem.suspQ = NULL;
  
  /* 初始化被信号量类型 */
  sem.type = SEM_COUNTY;

  return sem;
}

EASYRTOS_SEM eSemCreateBinary (void)
{
  EASYRTOS_SEM sem;

  /* 设置初始计数 */
  sem.count = 0;

  /* 初始化被信号量悬挂的任务的队列 */
  sem.suspQ = NULL;
  
  /* 初始化被信号量类型 */
  sem.type = SEM_BINARY;

  return sem;
}

EASYRTOS_SEM eSemCreateMutex (void)
{
    EASYRTOS_SEM sem;

    /* 初始化时没有owner */
    sem.owner = NULL;

    /* 初始化计数 */
    sem.count = 1;

    /* 初始化被其悬挂的任务 */
    sem.suspQ = NULL;
    
    /* 初始化被信号量类型 */
    sem.type = SEM_MUTEX;
    return (sem);
}

/**
 * 返回 EASYRTOS_OK 成功
 * 返回 EASYRTOS_ERR_QUEUE 将任务放置到Ready队列中失败
 * 返回 EASYRTOS_ERR_TIMER 取消定时器失败
 * 返回 EASYRTOS_ERR_PARAM 输入参数错误
 * 返回 EASYRTOS_ERR_DELETED 信号量在悬挂任务时被删除
 */
ERESULT eSemDelete (EASYRTOS_SEM *sem)
{
  ERESULT status;
  CRITICAL_STORE;
  EASYRTOS_TCB *tcb_ptr;
  uint8_t woken_threads = FALSE;

  /* 参数检查 */
  if (sem == NULL)
  {
    status = EASYRTOS_ERR_PARAM;
  }
  else
  {
    status = EASYRTOS_OK;

    /* 唤醒所有被悬挂的任务 */
    while (1)
    {
      /* 进入临界区 */
      CRITICAL_ENTER ();

      /* 检查是否有任务被悬挂 可能有很多任务被该信号量悬挂 &sem->suspQ为悬挂链表 */
      tcb_ptr = tcb_dequeue_head (&sem->suspQ);

      /* 若有任务被信号量悬挂 */
      if (tcb_ptr)
      {
        /* 对被悬挂的任务返回错误标志 */
        tcb_ptr->pendedWakeStatus = EASYRTOS_ERR_DELETED;

        /* 将任务TCB加入Ready的链表 */
        if (tcbEnqueuePriority (&tcb_readyQ, tcb_ptr) != EASYRTOS_OK)
        {
          /* 若加入失败，退出临界区 */
          CRITICAL_EXIT ();

          /* 退出循环，返回加入Ready链表失败 */
          status = EASYRTOS_ERR_QUEUE;
          break;
        }

        /* 若悬挂有timeout，取消对应的定时器 */
        if (tcb_ptr->pended_timo_cb)
        {
          if (eTimerCancel (tcb_ptr->pended_timo_cb) != EASYRTOS_OK)
          {
            /* 取消定时器失败，退出临界区 */
            CRITICAL_EXIT ();

            /* 退出循环，返回错误标识 */
            status = EASYRTOS_ERR_TIMER;
            break;
          }

          /* 标志没有timeout定时器注册 */
          tcb_ptr->pended_timo_cb = NULL;
        }

        /* 退出临界区 */
        CRITICAL_EXIT ();

        /* 请求调度器 */
        woken_threads = TRUE;
      }

      /* 没有被悬挂的任务了 */
      else
      {
        /* 退出临界区并结束循环 */
        CRITICAL_EXIT ();
        break;
      }
    }

    /* 有任务被唤醒则调用调度器 */
    if (woken_threads == TRUE)
    {
      /**
       *  只有在任务中才运行调度器，在中断中时会在退出中断
       *  时eIntExit()调用
       */
      if (eCurrentContext())
          easyRTOSSched (FALSE);
    }
  }

  return (status);
}

/**
 *  返回 EASYRTOS_OK 成功
 *  返回 EASYRTOS_TIMEOUT 信号量timeout到期
 *  返回 EASYRTOS_WOULDBLOCK 技术为0的时候，timeout=-1
 *  返回 EASYRTOS_ERR_DELETED 信号量在悬挂任务时被删除
 *  返回 EASYRTOS_ERR_CONTEXT 错误的上下文调用
 *  返回 EASYRTOS_ERR_PARAM  错误的参数
 *  返回 EASYRTOS_ERR_QUEUE 将任务加入运行队列失败
 *  返回 EASYRTOS_ERR_TIMER 注册没有成功
 *  返回 EASYRTOS_SEM_UINIT 信号量没有被初始化
 */
ERESULT eSemTake (EASYRTOS_SEM *sem, int32_t timeout)
{
  CRITICAL_STORE;
  ERESULT status;
  SEM_TIMER timerData;
  EASYRTOS_TIMER timerCb;
  EASYRTOS_TCB *curr_tcb_ptr;

  /* 参数检查 */
  if (sem == NULL)
  {
    status = EASYRTOS_ERR_PARAM;
  }
  else if (sem->type == NULL)
  {
    status = EASYRTOS_SEM_UINIT;
  }
  else
  {
    /* 获取正在运行任务的TCB */
    curr_tcb_ptr = eCurrentContext();
        
    /* 进入临界区 */
    CRITICAL_ENTER ();
    
    /**
     * 检测是否在任务上下文(而不是中断),因为MUTEX需要一个拥有者,所以不能
     * 被ISR调用
     */
    if (curr_tcb_ptr == NULL)
    {
        /* 退出临界区 */
        CRITICAL_EXIT ();

        /* 不在任务上下文中,无法悬挂任务 */
        status = EASYRTOS_ERR_CONTEXT;
    }

    /** 
     * 当为二值信号或者计数信号量的时候,则判断count是否为0.
     * 若为互斥锁信号量,则判断是否上下文与拥有任务不相同.
     * 满足其一,则悬挂该任务. 
     */
    else if (((sem->type != SEM_MUTEX) && (sem->count == 0)) || ((sem->type == SEM_MUTEX) && (sem->owner != curr_tcb_ptr) && (sem->owner != NULL)))
    {
      /* 若timeout >= 0 则悬挂任务 */
      if (timeout >= 0)
      {
        /* Count为0, 悬挂任务 */

        /* 若是在任务上下文中 */
        if (curr_tcb_ptr)
        {
          /* 将该任务加入该信号量的悬挂列表 */
          if (tcbEnqueuePriority (&sem->suspQ, curr_tcb_ptr) != EASYRTOS_OK)
          {
            /* 若失败，退出临界区 */
            CRITICAL_EXIT ();

            /* 返回错误 */
            status = EASYRTOS_ERR_QUEUE;
          }
          else
          {
            /* 将任务状态设置为悬挂 */
            curr_tcb_ptr->state = TASK_PENDED;
            
            status = EASYRTOS_OK;

            /* 根据timeout的值，决定是否需要注册定时器回调 */
            if (timeout)
            {
              /* 保存回调需要的数据 */
              timerData.tcb_ptr = curr_tcb_ptr;
              timerData.sem_ptr = sem;

              /* 定时器回调需要的数据 */
              timerCb.cb_func = eSemTimerCallback;
              timerCb.cb_data = (POINTER)&timerData;
              timerCb.cb_ticks = timeout;

              /**
               * 保存定时器回调在TCB中，当信号量GIVE的时候，方便取消
               * timeout注册的定时器
               */
              curr_tcb_ptr->pended_timo_cb = &timerCb;

              /* 注册timeout的定时器 */
              if (eTimerRegister (&timerCb) != EASYRTOS_OK)
              {
                /* 若注册失败，返回错误 */
                status = EASYRTOS_ERR_TIMER;

                /* 清除队列 */
                (void)tcb_dequeue_entry (&sem->suspQ, curr_tcb_ptr);
                curr_tcb_ptr->state = TASK_READY;
                curr_tcb_ptr->pended_timo_cb = NULL;
              }
            }

            /* 没有timeout请求 */
            else
            {
              curr_tcb_ptr->pended_timo_cb = NULL;
            }

            /* 退出临界区 */
            CRITICAL_EXIT ();

            /* 检查是否有错误发生 */
            if (status == EASYRTOS_OK)
            {
              /* 任务被悬挂，调用调度器启动一个新的任务 */
              easyRTOSSched (FALSE);

              /**
               * 通过eSemGive()唤醒会返回EASYRTOS_OK，当timeout时间到返回
               * EASYRTOS_TIMEOUT，当信号量被删除时，返回EASYRTOS_ERR_DELETED
               */
              status = curr_tcb_ptr->pendedWakeStatus;

              /**
               * 若线程在EASYRTOS_OK的情况下被唤醒时，其他任务增加了信号量
               * 计数并把控制权交给该任务。理论上，之前的任务增加信号量计数，
               * 然后这个任务减少信号量计数，但是为了能够让其他优先级更高的
               * 任务抢占，我们把减少计数的地方放在了eSemGive()中
               */
            }
          }
        }
        else
        {
          /* 退出临界区 */
          CRITICAL_EXIT ();

          /* 不在任务上下文，不能悬挂 */
          status = EASYRTOS_ERR_CONTEXT;
        }
      }
      else
      {
        /* timeout == -1, 不需要悬挂 */
        CRITICAL_EXIT();
        status = EASYRTOS_WOULDBLOCK;
      }
    }
    else
    {
      switch (sem->type)
      {
        case SEM_BINARY:
        case SEM_COUNTY:
            sem->count--;
            status = EASYRTOS_OK;
          break;
        case SEM_MUTEX:
          
          /* Count不是0，减少Count的值，并返回 */
          if (sem->owner == NULL)
          {
            sem->owner = curr_tcb_ptr;
          }
      
          /* Count不是0，减少Count的值，并返回 */
          if (sem->count>-127)
          {
            sem->count--;
        
            /* 成功 */
            status = EASYRTOS_OK;
          }
          else {
            status = EASYRTOS_ERR_OVF;
          }
          break;
        default:
          status = EASYRTOS_SEM_UINIT;
      }

      /* 退出临界区 */
      CRITICAL_EXIT ();
  
    }
  }

  return (status);
}


/**
 * 返回 EASYRTOS_OK 成功
 * 返回 EASYRTOS_ERR_OVF 计数信号量count>127(>127)
 * 返回 EASYRTOS_ERR_PARAM 错误的参数
 * 返回 EASYRTOS_ERR_QUEUE 将任务加入运行队列失败
 * 返回 EASYRTOS_ERR_TIMER 注册定时器未成功
 * 返回 EASYRTOS_ERR_BIN_OVF 二值信号量count已经为1
 * 返回 EASYRTOS_SEM_UINIT 信号量没有被初始化
 * 返回 EASYRTOS_ERR_OWNERSHIP 尝试解锁Mutex的任务不是Mutex拥有者
 */
ERESULT eSemGive (EASYRTOS_SEM * sem)
{
  ERESULT status;
  CRITICAL_STORE;
  EASYRTOS_TCB *tcb_ptr;
  EASYRTOS_TCB *curr_tcb_ptr;
  /* 参数检查 */
  if (sem == NULL)
  {
    status = EASYRTOS_ERR_PARAM;
  }
  else if (sem->type == NULL)
  {
    status = EASYRTOS_SEM_UINIT;
  }
  else
  {
    /* 获取正在运行的任务的TCB */
    curr_tcb_ptr = eCurrentContext();
        
    /* 进入临界区 */
    CRITICAL_ENTER ();
    
    if (sem->type == SEM_MUTEX && sem->owner != curr_tcb_ptr)
    {
        /* 退出临界区 */
        CRITICAL_EXIT ();
        
        status = EASYRTOS_ERR_OWNERSHIP;
    }

    /* 将被信号量悬挂的任务置入Ready任务列表 */
    else 
    {
      
      if (sem->suspQ && sem->count == 0)
      {
        sem->owner = NULL;
        //if ( sem->type == SEM_MUTEX )sem->count++;
        tcb_ptr = tcb_dequeue_head (&sem->suspQ);
        if (tcbEnqueuePriority (&tcb_readyQ, tcb_ptr) != EASYRTOS_OK)
        {
          /* 若加入Ready列表失败，退出临界区 */
          CRITICAL_EXIT ();

          status = EASYRTOS_ERR_QUEUE;
        }
        else
        {
          /* 给等待的任务返回EASYRTOS_OK */
          tcb_ptr->pendedWakeStatus = EASYRTOS_OK;

          /* 设置任务为新的互斥锁ower */
          sem->owner = tcb_ptr;
          
          /* 解除该信号量timeout注册的定时器 */
          if ((tcb_ptr->pended_timo_cb != NULL)
              && (eTimerCancel (tcb_ptr->pended_timo_cb) != EASYRTOS_OK))
          {
              /* 解除定时器失败 */
              status = EASYRTOS_ERR_TIMER;
          }
          else
          {
              /* 没有timeout定时器注册 */
              tcb_ptr->pended_timo_cb = NULL;

              /* 成功 */
              status = EASYRTOS_OK;
          }

          /* 退出临界区 */
          CRITICAL_EXIT ();

          if (eCurrentContext())
              easyRTOSSched (FALSE);
        }
      }
    

      /* 若没有任务被该信号量悬挂，则增加count，然后返回 */
      else
      {
        switch (sem->type)
        {
          case SEM_COUNTY:
            /* 检查是否溢出 */
            if (sem->count == 127)
            {
              /* 返回错误标识 */
              status = EASYRTOS_ERR_OVF;
            }
            else
            {
              /* 增加count并返回 */
              sem->count++;
              status = EASYRTOS_OK;
            }
          break;
          
          case SEM_BINARY:
            /* 检查是否已经为1 */
            if (sem->count)
            {
              /* 返回错误标识 */
              status = EASYRTOS_ERR_BIN_OVF;
            }
            else
            {
              /* 增加count并返回 */
              sem->count = 1;
              status = EASYRTOS_OK;
            }
          break;
          
          case SEM_MUTEX:
            if (sem->count>1)
            {
              /* 返回错误标识 */
              status = EASYRTOS_ERR_BIN_OVF;
            }
            else
            {
              sem->count++;
              status = EASYRTOS_OK;
            }
          break;
        }
      }

      /* 退出临界区 */
      CRITICAL_EXIT ();
    }
  }

  return (status);
}


/**
 * 返回 EASYRTOS_OK 成功
 * 返回 EASYRTOS_ERR_PARAM 错误的参数
 */
ERESULT eSemResetCount (EASYRTOS_SEM *sem, uint8_t count)
{
  uint8_t status;

  /* 参数检查 */
  if (sem == NULL)
  {
    status = EASYRTOS_ERR_PARAM;
  }
  else
  {
    /* 设置count值 */
    sem->count = count;

    /* 成功 */
    status = EASYRTOS_OK;
  }
  return (status);  
}

static void eSemTimerCallback (POINTER cb_data)
{
    SEM_TIMER *timer_data_ptr;
    CRITICAL_STORE;

    /* 获取SEM_TIMER结构指针 */
    timer_data_ptr = (SEM_TIMER *)cb_data;

    /* 检查参数是否为空 */
    if (timer_data_ptr)
    {
      /* 进入临界区 */
      CRITICAL_ENTER ();

      /* 设置标志，表明任务是由于timeout到期而唤醒的  */
      timer_data_ptr->tcb_ptr->pendedWakeStatus = EASYRTOS_TIMEOUT ;

      /* 解除timeout定时器注册 */
      timer_data_ptr->tcb_ptr->pended_timo_cb = NULL;

      /* 将任务移除信号量悬挂队列 */
      (void)tcb_dequeue_entry (&timer_data_ptr->sem_ptr->suspQ, timer_data_ptr->tcb_ptr);

      /* 将任务加入Ready队列 */
      (void)tcbEnqueuePriority (&tcb_readyQ, timer_data_ptr->tcb_ptr);

      /* 退出临界区 */
      CRITICAL_EXIT ();

      /**
       * 这里没有启动调度器，因为之后在退出timer ISR的时候会通过atomIntExit()启动
       */
    }
}
