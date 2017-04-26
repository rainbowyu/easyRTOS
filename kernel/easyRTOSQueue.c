/**
 * 作者: Roy.yu
 * 时间: 2017.04.26
 * 版本: V1.2
 * Licence: GNU GENERAL PUBLIC LICENSE
 */
#include "easyRTOS.h"
#include "easyRTOSkernel.h"
#include "easyRTOSport.h"
#include "easyRTOSTimer.h"
#include "easyRTOSQueue.h"

#include "string.h"

/* 私有函数 */
static ERESULT queue_remove (EASYRTOS_QUEUE *qptr, void* msgptr);
static ERESULT queue_insert (EASYRTOS_QUEUE *qptr, void* msgptr);
static void eQueueTimerCallback (POINTER cb_data);

/* 全局函数 */
EASYRTOS_QUEUE eQueueCreate ( void *buff_ptr, uint32_t unit_size, uint32_t max_num_msgs);
ERESULT eQueueDelete (EASYRTOS_QUEUE *qptr);
ERESULT eQueueTake (EASYRTOS_QUEUE *qptr, int32_t timeout, void *msgptr);
ERESULT eQueueGive (EASYRTOS_QUEUE *qptr, int32_t timeout, void *msgptr);

/**
 * 功能: 队列创建,初始化队列结构体内的参数,并返回.
 *
 * 参数:
 * 输入:                                   输出:
 * void *buff_ptr 队列消息保存地址指针      无
 * uint32_t unit_size 单个消息所占字节
 * uint32_t max_num_msgs 总消息数量
 *
 * 返回:
 * EASYRTOS_QUEUE
 * 
 * 调用的函数:
 * 无.
 */
EASYRTOS_QUEUE eQueueCreate ( void *buff_ptr, uint32_t unit_size, uint32_t max_num_msgs)
{
  EASYRTOS_QUEUE qptr;

  /* 存储队列详情 */
  qptr.buff_ptr = buff_ptr;
  qptr.unit_size = unit_size;
  qptr.max_num_msgs = max_num_msgs;

  /* 初始化被阻塞任务队列 */
  qptr.putSuspQ = NULL;
  qptr.getSuspQ = NULL;

  /* 初始化插入/移除索引 */
  qptr.insert_index = 0;
  qptr.remove_index = 0;
  qptr.num_msgs_stored = 0;

  return (qptr);
}

/**
 * 功能: 删除队列,并唤醒所有被该队列悬挂的任务加入Ready列表中.同时取消该任务
 * 注册的定时器.若有任务被唤醒,则会启动调度器.
 *
 * 参数:
 * 输入:                                   输出:
 * EASYRTOS_QUEUE *qptr 队列指针           EASYRTOS_QUEUE *qptr 队列指针
 *
 * 返回:
 * EASYRTOS_OK 成功
 * EASYRTOS_ERR_QUEUE 将任务放置到Ready队列中失败
 * EASYRTOS_ERR_TIMER 取消定时器失败
 * 
 * 调用的函数:
 * tcb_dequeue_head (&qptr->getSuspQ);
 * tcb_dequeue_head (&qptr->putSuspQ);
 * tcbEnqueuePriority (&tcb_readyQ, tcb_ptr);
 * eTimerCancel (tcb_ptr->pended_timo_cb);
 * eCurrentContext();
 * easyRTOSSched (FALSE);
 */
ERESULT eQueueDelete (EASYRTOS_QUEUE *qptr)
{
  ERESULT status;
  CRITICAL_STORE;
  EASYRTOS_TCB *tcb_ptr;
  uint8_t wokenTasks = FALSE;

  /* 参数检查 */
  if (qptr == NULL)
  {
    status = EASYRTOS_ERR_PARAM;
  }
  else
  {
    /* 默认返回 */
    status = EASYRTOS_OK;

    /* 唤醒所有被悬挂的任务（将其加入Ready队列） */
    while (1)
    {
      /* 进入临界区 */
      CRITICAL_ENTER ();

      /* 检查是否有线程被悬挂 （等待发送或等待接收） */
      if (((tcb_ptr = tcb_dequeue_head (&qptr->getSuspQ)) != NULL)
          || ((tcb_ptr = tcb_dequeue_head (&qptr->putSuspQ)) != NULL))
      {

        /* 返回错误状态 */
        tcb_ptr->pendedWakeStatus = EASYRTOS_ERR_DELETED;

        /* 将任务加入Ready队列 */
        if (tcbEnqueuePriority (&tcb_readyQ, tcb_ptr) != EASYRTOS_OK)
        {
          /* 退出临界区 */
          CRITICAL_EXIT ();

          /* 退出循环，返回错误 */
          status = EASYRTOS_ERR_QUEUE;
          break;
        }
        else tcb_ptr->state = TASK_READY;

        /* 取消阻塞任务注册的定时器 */
        if (tcb_ptr->pended_timo_cb)
        {
          /* 取消回调函数 */
          if (eTimerCancel (tcb_ptr->pended_timo_cb) != EASYRTOS_OK)
          {
              /* 退出临界区 */
              CRITICAL_EXIT ();

              /* 退出循环，返回错误 */
              status = EASYRTOS_ERR_TIMER;
              break;
          }

          /* 标记任务没有定时器回调 */
          tcb_ptr->pended_timo_cb = NULL;
        }

        /* 退出临界区 */
        CRITICAL_EXIT ();

        /* 是否调用调度器 */
        wokenTasks= TRUE;
      }

      /* 没有被悬挂的任务 */
      else
      {
        /* 退出临界区 */
        CRITICAL_EXIT ();
        break;
      }
    }

    /* 若有任务被唤醒，调用调度器 */
    if (wokenTasks == TRUE)
    {
      /**
       * 只在任务上下文环境调用调度器。
       * 中断环境会有eIntExit()调用调度器。
       */
      if (eCurrentContext())
        easyRTOSSched (FALSE);
    }
  }

  return (status);
}

/**
 * 功能: 取出队列中的数据,若对队列为空,根据timeout的不同值有不同的处理方式.
 * 1.timeout>0 悬挂调用的任务,当timeout到期的时候唤醒任务并返回timeout标志
 * 2.timeout=0 永久悬挂调用的任务,直到从队列中等到数据.
 * 3.timeout=-1 不悬挂任务,若队列为空会返回队列为空标志.
 * 当有任务被悬挂的时候,将会调用调度器.
 *
 * 参数:
 * 输入:                                        输出:
 * EASYRTOS_QUEUE *qptr 队列指针                EASYRTOS_QUEUE *qptr 队列指针
 * int32_t timeout timeout时间,依赖于心跳时间   void *msgptr 获取的消息
 * void *msgptr 获取的消息                 
 * 
 * 返回:
 * EASYRTOS_OK 成功
 * EASYRTOS_TIMEOUT 信号量timeout到期
 * EASYRTOS_WOULDBLOCK 本来会被悬挂但由于timeout为-1所以返回了
 * EASYRTOS_ERR_DELETED 队列在悬挂任务时被删除
 * EASYRTOS_ERR_CONTEXT 错误的上下文调用
 * EASYRTOS_ERR_PARAM 参数错误
 * EASYRTOS_ERR_QUEUE 将任务加入运行队列失败
 * EASYRTOS_ERR_TIMER 注册定时器未成功
 * 
 * 调用的函数:
 * eCurrentContext();
 * tcbEnqueuePriority (&qptr->getSuspQ, curr_tcb_ptr);
 * eTimerRegister (&timerCb);
 * tcb_dequeue_entry (&qptr->getSuspQ, curr_tcb_ptr);
 * easyRTOSSched (FALSE);
 * queue_remove (qptr, msgptr);
 */
ERESULT eQueueTake (EASYRTOS_QUEUE *qptr, int32_t timeout, void *msgptr)
{
    CRITICAL_STORE;
    ERESULT status;
    QUEUE_TIMER timerData;
    EASYRTOS_TIMER timerCb;
    EASYRTOS_TCB *curr_tcb_ptr;

    /* 参数检查 */
    if ((qptr == NULL)) //|| (msgptr == NULL))
    {
      status = EASYRTOS_ERR_PARAM;
    }
    else
    {
      /* 进入临界区 */
      CRITICAL_ENTER ();

      /* 若队列中没有消息，则悬挂任务 */
      if (qptr->num_msgs_stored == 0)
      {
        
        /* timeout>0 悬挂任务 */
        if (timeout >= 0)
        {

          /* 获取当前任务TCB */
          curr_tcb_ptr = eCurrentContext();

          /* 检查我们是否在任务上下文 */
          if (curr_tcb_ptr)
          {
            
            /* 将当前任务增加到receive悬挂队列中 */
            if (tcbEnqueuePriority (&qptr->getSuspQ, curr_tcb_ptr) == EASYRTOS_OK)
            {
              
              /* 将任务状态设置为悬挂 */
              curr_tcb_ptr->state = TASK_PENDED;

              status = EASYRTOS_OK;

              /* 注册定时器回调 */
              if (timeout)
              {
                /* 填充定时器需要的数据 */
                timerData.tcb_ptr = curr_tcb_ptr;
                timerData.queue_ptr = qptr;
                timerData.suspQ = &qptr->getSuspQ;

                /* 填充回调需要的数据 */
                timerCb.cb_func = eQueueTimerCallback;
                timerCb.cb_data = (POINTER)&timerData;
                timerCb.cb_ticks = timeout;

                /* 在任务TCB中存储定时器回调，方便对其进行取消操作 */
                curr_tcb_ptr->pended_timo_cb = &timerCb;

                /* 注册定时器 */
                if (eTimerRegister (&timerCb) != EASYRTOS_OK)
                {
                  /* 注册失败 */
                  status = EASYRTOS_ERR_TIMER;

                  (void)tcb_dequeue_entry (&qptr->getSuspQ, curr_tcb_ptr);
                  curr_tcb_ptr->state = TASK_RUN;
                  curr_tcb_ptr->pended_timo_cb = NULL;
                }
              }

              /* 不需要注册定时器 */
              else
              {
                curr_tcb_ptr->pended_timo_cb = NULL;
              }

              /* 退出临界区 */
              CRITICAL_EXIT();
              
              if (status == EASYRTOS_OK)
              {
                /* 当前任务被悬挂，我们将调用调度器 */
                easyRTOSSched (FALSE);
                
                /* 下次任务将从此处开始运行，此时队列被删除 或者timeout到期 或者调用了eQueueGive */
                status = curr_tcb_ptr->pendedWakeStatus;

                /** 
                 * 检测pendedWakeStatus，若其值为EASYRTOS_OK，则说明
                 * 读取是成功的，若为其他的值，则说明有可能队列被删除
                 * 或者timeout到期，此时我们只需要退出就好了
                 */
                if (status == EASYRTOS_OK)
                {
                  /* 进入临界区 */
                  CRITICAL_ENTER();

                  /* 将消息复制出来 */
                  status = queue_remove (qptr, msgptr);

                  /* 退出临界区 */
                  CRITICAL_EXIT();
                }
              }
            }
            else
            {
              /* 将任务加入悬挂列表失败 */
              CRITICAL_EXIT ();
              status = EASYRTOS_ERR_QUEUE;
            }
          }
          else
          {
            /* 不在任务上下文中们，无法悬挂任务 */
            CRITICAL_EXIT ();
            status = EASYRTOS_ERR_CONTEXT;
          }
        }
        else
        {
          /* timeout == -1, 不需要悬挂任务，且队列此时数据量为0 */
          CRITICAL_EXIT();
          status = EASYRTOS_WOULDBLOCK;
        }
      }
      else
      {
        /* 不需要阻塞任务，直接把消息复制出来 */
        status = queue_remove (qptr, msgptr);

        /* 退出临界区 */
        CRITICAL_EXIT ();

        /**
         * 只在任务上下文环境调用调度器。
         * 中断环境会有eIntExit()调用调度器。.
         */
        if (eCurrentContext())
          easyRTOSSched (FALSE);
      }
    }

    return (status);
}

/**
 * 功能: 取出队列中的数据,若对队列为空,根据timeout的不同值有不同的处理方式.
 * 1.timeout>0 悬挂调用的任务,当timeout到期的时候唤醒任务并返回timeout标志
 * 2.timeout=0 永久悬挂调用的任务,直到从队列中等到数据.
 * 3.timeout=-1 不悬挂任务,若队列为空会返回队列为空标志.
 * 当有任务被悬挂的时候,将会调用调度器.
 *
 * 参数:
 * 输入:                                        输出:
 * EASYRTOS_QUEUE *qptr 队列指针                EASYRTOS_QUEUE *qptr 队列指针
 * int32_t timeout timeout时间,依赖于心跳时间   void *msgptr 放入队列的消息
 * void *msgptr 放入队列的消息                
 * 
 * 返回:
 * EASYRTOS_OK 成功
 * EASYRTOS_WOULDBLOCK 本来会被悬挂但由于timeout为-1所以返回了
 * EASYRTOS_TIMEOUT 信号量timeout到期
 * EASYRTOS_ERR_DELETED 队列在悬挂任务时被删除
 * EASYRTOS_ERR_CONTEXT 错误的上下文调用
 * EASYRTOS_ERR_PARAM 参数错误
 * EASYRTOS_ERR_QUEUE 将任务加入运行队列失败
 * EASYRTOS_ERR_TIMER 注册定时器未成功
 * 
 * 调用的函数:
 * eCurrentContext();
 * tcbEnqueuePriority (&qptr->getSuspQ, curr_tcb_ptr);
 * eTimerRegister (&timerCb);
 * tcb_dequeue_entry (&qptr->getSuspQ, curr_tcb_ptr);
 * easyRTOSSched (FALSE);
 * queue_remove (qptr, msgptr);
 */
ERESULT eQueueGive (EASYRTOS_QUEUE *qptr, int32_t timeout, void *msgptr)
{
    CRITICAL_STORE;
    ERESULT status;
    QUEUE_TIMER timerData;
    EASYRTOS_TIMER timerCb;
    EASYRTOS_TCB *curr_tcb_ptr;

    /* 参数检查 */
    if ((qptr == NULL) || (msgptr == NULL))
    {
        status = EASYRTOS_ERR_PARAM;
    }
    else
    {
        /* 进入临界区 */
        CRITICAL_ENTER ();

        /* 若队列已满，悬挂调用此函数的任务 */
        if (qptr->num_msgs_stored == qptr->max_num_msgs)
        {
            /* timeout >= 0, 任务将被悬挂 */
            if (timeout >= 0)
            {

                /* 获取当前任务TCB */
                curr_tcb_ptr = eCurrentContext();

                /* 检查是否在任务上下文 */
                if (curr_tcb_ptr)
                {
                    /* 将当前任务添加到send悬挂列表中 */
                    if (tcbEnqueuePriority (&qptr->putSuspQ, curr_tcb_ptr) == EASYRTOS_OK)
                    {
                        /* 设置任务状态标志位为悬挂 */
                        curr_tcb_ptr->state = TASK_PENDED;

                        status = EASYRTOS_OK;

                        /* timeout>0 注册定时器回调 */
                        if (timeout)
                        {
                            /* 填充定时器需要的数据 */
                            timerData.tcb_ptr = curr_tcb_ptr;
                            timerData.queue_ptr = qptr;
                            timerData.suspQ = &qptr->putSuspQ;


                            /* 填充回调需要的数据 */
                            timerCb.cb_func = eQueueTimerCallback;
                            timerCb.cb_data = (POINTER)&timerData;
                            timerCb.cb_ticks = timeout;

                            /* 在任务TCB中存储定时器回调，方便对其进行取消操作 */
                            curr_tcb_ptr->pended_timo_cb = &timerCb;

                            /* 注册定时器 */
                            if (eTimerRegister (&timerCb) != EASYRTOS_OK)
                            {
                                /* 注册失败 */
                                status = EASYRTOS_ERR_TIMER;
                                
                                (void)tcb_dequeue_entry (&qptr->putSuspQ, curr_tcb_ptr);
                                curr_tcb_ptr->state = TASK_RUN;
                                curr_tcb_ptr->pended_timo_cb = NULL;
                            }
                        }

                        /* 不需要注册定时器 */
                        else
                        {
                            curr_tcb_ptr->pended_timo_cb = NULL;
                        }

                        /* 退出临界区 */
                        CRITICAL_EXIT ();

                        /* 检测是否注册成功 */
                        if (status == EASYRTOS_OK)
                        {
                            /* 当前任务被悬挂，我们将调用调度器 */
                            easyRTOSSched (FALSE);
                            
                            /* 下次任务将从此处开始运行，此时队列被删除 或者timeout到期 或者调用了eQueueGive */
                            status = curr_tcb_ptr->pendedWakeStatus;

                            /** 
                             * 检测pendedWakeStatus，若其值为EASYRTOS_OK，则说明
                             * 读取是成功的，若为其他的值，则说明有可能队列被删除
                             * 或者timeout到期，此时我们只需要退出就好了
                             */
                            if (status == EASYRTOS_OK)
                            {
                                /* 进入临界区 */
                                CRITICAL_ENTER();

                                /* 将消息加入队列 */
                                status = queue_insert (qptr, msgptr);

                                /* 退出临界区 */
                                CRITICAL_EXIT();
                            }
                        }
                    }
                    else
                    {
                        /* 将任务加入悬挂队列失败 */
                        CRITICAL_EXIT();
                        status = EASYRTOS_ERR_QUEUE;
                    }
                }
                else
                {
                    /* 不再任务上下文，不能悬挂任务 */
                    CRITICAL_EXIT ();
                    status = EASYRTOS_ERR_CONTEXT;
                }
            }
            else
            {
                /* timeout == -1, 不需要悬挂任务，且队列此时数据量为0 */
                CRITICAL_EXIT();
                status = EASYRTOS_WOULDBLOCK;
            }
        }
        else
        {
            /* 不用悬挂任务，直接将数据复制进队列 */
            status = queue_insert (qptr, msgptr);

            /* 退出临界区 */
            CRITICAL_EXIT ();

            /**
             * 只在任务上下文环境调用调度器。
             * 中断环境会有eIntExit()调用调度器。.
             */
            if (eCurrentContext())
                easyRTOSSched (FALSE);
        }
    }

    return (status);
}

/**
 * 功能: 队列注册的定时器的回调函数,给到期的任务返回EASYRTOS_TIMEOUT的标志.
 * 将到期的任务移除队列悬挂列表,并加入Ready列表.
 *
 * 参数:
 * 输入:                                                输出:
 * POINTER cb_data 回调函数数据包含需要唤醒的TCB等信息   POINTER cb_data 回调函数数据包含需要唤醒的TCB等信息                              
 * 
 * 返回:void
 * 
 * 调用的函数:
 * (void)tcb_dequeue_entry (timer_data_ptr->suspQ, timer_data_ptr->tcb_ptr);
 * (void)tcbEnqueuePriority (&tcb_readyQ, timer_data_ptr->tcb_ptr);
 */
static void eQueueTimerCallback (POINTER cb_data)
{
    QUEUE_TIMER *timer_data_ptr;
    CRITICAL_STORE;

    /* 获取队列定时器回调指针 */
    timer_data_ptr = (QUEUE_TIMER *)cb_data;

    /* 检测参数是否为空 */
    if (timer_data_ptr)
    {
        /* 进入临界区  */
        CRITICAL_ENTER ();

        /* 给任务设置标志位，标识任务是因为定时器回调唤醒的 */
        timer_data_ptr->tcb_ptr->pendedWakeStatus = EASYRTOS_TIMEOUT;

        /* 取消定时器注册 */
        timer_data_ptr->tcb_ptr->pended_timo_cb = NULL;

        /* 将任务移除悬挂列表，将任务移出receive或者send列表 */
        (void)tcb_dequeue_entry (timer_data_ptr->suspQ, timer_data_ptr->tcb_ptr);

        /* 将任务加入Ready队列 */
        if (tcbEnqueuePriority (&tcb_readyQ, timer_data_ptr->tcb_ptr) == EASYRTOS_OK)
        {
          timer_data_ptr->tcb_ptr->state= TASK_READY;
        }

        /* 退出临界区 */
        CRITICAL_EXIT ();
    }
}

/**
 * 功能: 取出队列中的消息,若有任务正在等待发送(取出消息后,队列将多一个空位),则唤醒任
 * 务并取消其注册的定时器.
 *
 * 参数:
 * 输入:                                输出:
 * EASYRTOS_QUEUE *qptr 队列指针        EASYRTOS_QUEUE *qptr 队列指针                              
 * void* msgptr 取出的消息              void* msgptr 取出的消息
 *
 * 返回:
 * EASYRTOS_OK 成功
 * EASYRTOS_ERR_PARAM 参数错误
 * EASYRTOS_ERR_QUEUE 将任务加入运行队列失败
 * EASYRTOS_ERR_TIMER 取消定时器失败
 *
 * 调用的函数:
 * memcpy ((uint8_t*)msgptr, ((uint8_t*)qptr->buff_ptr + qptr->remove_index), qptr->unit_size);
 * tcb_dequeue_head (&qptr->putSuspQ);
 * tcbEnqueuePriority (&tcb_readyQ, tcb_ptr);
 * eTimerCancel (tcb_ptr->pended_timo_cb);
 */
static ERESULT queue_remove (EASYRTOS_QUEUE *qptr, void* msgptr)
{
  ERESULT status;
  EASYRTOS_TCB *tcb_ptr;

  /* 参数检查 */
  if ((qptr == NULL)) //|| (msgptr == NULL))
  {
    status = EASYRTOS_ERR_PARAM;
  }
  else
  {
    /* 队列中有数据，将其复制出来 */
    memcpy ((uint8_t*)msgptr, ((uint8_t*)qptr->buff_ptr + qptr->remove_index), qptr->unit_size);
    qptr->remove_index += qptr->unit_size;
    qptr->num_msgs_stored--;
    
    /* 队列为循环存储数据，目的是为了加快查找速度 */
    /* 检查是否重置remove_index */
    if (qptr->remove_index >= (qptr->unit_size * qptr->max_num_msgs))
        qptr->remove_index = 0;

    /* 若有任务正在等待发送，则将其唤醒 */    
    tcb_ptr = tcb_dequeue_head (&qptr->putSuspQ);
    if (tcb_ptr)
    {
      /* 将悬挂的任务加入Ready列表 */
      if (tcbEnqueuePriority (&tcb_readyQ, tcb_ptr) == EASYRTOS_OK)
      {
        
        tcb_ptr->pendedWakeStatus = EASYRTOS_OK;
        tcb_ptr->state = TASK_READY;
        
        /* 若注册了定时器回调，则先取消 */
        if ((tcb_ptr->pended_timo_cb != NULL)
            && (eTimerCancel (tcb_ptr->pended_timo_cb) != EASYRTOS_OK))
        {
          status = EASYRTOS_ERR_TIMER;
        }
        else
        {
          tcb_ptr->pended_timo_cb = NULL;

          /* 成功 */
          status = EASYRTOS_OK;
        }
      }
      else
      {
        /* 将任务加入Ready列表失败 */              
        status = EASYRTOS_ERR_QUEUE;
      }
    }
    else
    {
      /* 没有任务等待发送 */
      status = EASYRTOS_OK;
    }
  }

  return (status);
}

/**
 * 功能: 向队列中加入消息,若有任务等待接收消息(加入消息后则可获取消息),则唤醒任
 * 务并取消其注册的定时器.
 *
 * 参数:
 * 输入:                                输出:
 * EASYRTOS_QUEUE *qptr 队列指针        EASYRTOS_QUEUE *qptr 队列指针                              
 * void* msgptr 插入的消息             void* msgptr 插入的消息
 *
 * 返回:
 * EASYRTOS_OK 成功
 * EASYRTOS_ERR_PARAM 参数错误
 * EASYRTOS_ERR_QUEUE 将任务加入运行队列失败
 * EASYRTOS_ERR_TIMER 取消定时器失败
 *
 * 调用的函数:
 * memcpy (((uint8_t*)qptr->buff_ptr + qptr->insert_index), (uint8_t*)msgptr, qptr->unit_size);
 * tcb_dequeue_head (&qptr->getSuspQ)
 * tcbEnqueuePriority (&tcb_readyQ, tcb_ptr);
 * eTimerCancel (tcb_ptr->pended_timo_cb);
 */
static ERESULT queue_insert (EASYRTOS_QUEUE *qptr, void *msgptr)
{
    ERESULT status;
    EASYRTOS_TCB *tcb_ptr;

    /* 参数检查 */
    if ((qptr == NULL)|| (msgptr == NULL))
    {
        status = EASYRTOS_ERR_PARAM;
    }
    else
    {
        /* 队列中有空闲位置，将数据复制进去 */
        memcpy (((uint8_t*)qptr->buff_ptr + qptr->insert_index), (uint8_t*)msgptr, qptr->unit_size);
        qptr->insert_index += qptr->unit_size;
        qptr->num_msgs_stored++;

        /* 队列为循环存储数据，目的是为了加快查找速度 */
        /* 检查是否重置remove_index */
        if (qptr->insert_index >= (qptr->unit_size * qptr->max_num_msgs))
            qptr->insert_index = 0;

        /* 若有任务正在等待接收，则将其唤醒 */    
        tcb_ptr = tcb_dequeue_head (&qptr->getSuspQ);
        if (tcb_ptr)
        {
            /* 将悬挂的任务加入Ready列表 */
            if (tcbEnqueuePriority (&tcb_readyQ, tcb_ptr) == EASYRTOS_OK)
            {

                tcb_ptr->pendedWakeStatus = EASYRTOS_OK;
                tcb_ptr->state = TASK_READY;
                
                /* 若注册了定时器回调，则先取消 */
                if ((tcb_ptr->pended_timo_cb != NULL)
                    && (eTimerCancel (tcb_ptr->pended_timo_cb) != EASYRTOS_OK))
                {
                    status = EASYRTOS_ERR_TIMER;
                }
                else
                {
                  
                    tcb_ptr->pended_timo_cb = NULL;

                    /* 成功 */
                    status = EASYRTOS_OK;
                }
            }
            else
            {
                /* 将任务加入Ready列表失败 */  
                status = EASYRTOS_ERR_QUEUE;
            }
        }
        else
        {
            /* 没有任务等待接收 */
            status = EASYRTOS_OK;
        }
    }

    return (status);
}
