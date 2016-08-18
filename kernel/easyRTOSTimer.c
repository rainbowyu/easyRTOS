#include "easyRTOS.h"
#include "easyRTOSkernel.h"
#include "easyRTOSport.h"
#include "easyRTOSTimer.h"
/* 本地数据 */

/* timer队列的指针 */
static EASYRTOS_TIMER *timer_queue = NULL;

/**
 *  easyRTOS系统滴答数量
 */
static uint32_t systemTicks = 0;

/* 私有函数  */
static void eTimerCallbacks (void);
static void eTimerDelayCallback (POINTER cb_data);

/* 全局函数  */
void eTimerCallbacks (void);
void eTimerTick (void);
ERESULT eTimerRegister (EASYRTOS_TIMER *timer_ptr);
ERESULT eTimerCancel (EASYRTOS_TIMER *timer_ptr);
ERESULT eTimerDelay (uint32_t ticks);
uint32_t eTimeGet(void);
void eTimeSet(uint32_t newTime);

/**
 * 返回 EASYRTOS_OK
 * 返回 EASYRTOS_ERR_PARAM
 */
ERESULT eTimerRegister (EASYRTOS_TIMER *timer_ptr)
{
    ERESULT status;
    CRITICAL_STORE;

    /**
     *  检查参数
     */
    if ((timer_ptr == NULL) || (timer_ptr->cb_func == NULL)
        || (timer_ptr->cb_ticks == 0))
    {
        /** 
         *  错误返回
         */
        status = EASYRTOS_ERR_PARAM;
    }
    else
    {
        /** 
         *  进入临界区
         */
        CRITICAL_ENTER ();

        /*
         *  timer加入列表
         *
         *  列表是没有顺序的,所有的timer都从列表的头插入.每个系统心跳,该列表
         *  中的所有计数将会减少1,当count减到0,则timer的回掉函数会被启动.
         */
        if (timer_queue == NULL)
        {
            /** 
             *  列表为空,插入头列表的头
             */
            timer_ptr->next_timer = NULL;
            timer_queue = timer_ptr;
        }
        else
        {
            /** \
             *  列表已经有了timer,在列表头插入新的timer
             */
            timer_ptr->next_timer = timer_queue;
            timer_queue = timer_ptr;
        }

        /**
         *  退出临界区
         */
        CRITICAL_EXIT ();

        /** 
         *  注册成功 
         */
        status = EASYRTOS_OK;
    }

    return (status);
}


/**
 * 返回 EASYRTOS_OK
 * 返回 EASYRTOS_ERR_PARAM
 * 返回 EASYRTOS_ERR_NOT_FOUND
 */
ERESULT eTimerCancel (EASYRTOS_TIMER *timer_ptr)
{
    ERESULT status = EASYRTOS_ERR_NOT_FOUND;
    EASYRTOS_TIMER *prev_ptr, *next_ptr;
    CRITICAL_STORE;

    /** 
     *  检查参数
     *  Parameter check 
     */
    if (timer_ptr == NULL)
    {
        /** 
         *  参数错误返回
         */
        status = EASYRTOS_ERR_PARAM;
    }
    else
    {
        /**
         *  进入临界区
         */
        CRITICAL_ENTER ();

        /** 
         *  搜寻列表,找到相关的timer
         */
        prev_ptr = next_ptr = timer_queue;
        while (next_ptr)
        {
            /** 
             *  是否是我们寻找的timer
             */
            if (next_ptr == timer_ptr)
            {
                if (next_ptr == timer_queue)
                {
                    /** 
                     *  移除的是列表头
                     */
                    timer_queue = next_ptr->next_timer;
                }
                else
                {
                    /** 
                     *  移除的是列表中或者列表尾 
                     */
                    prev_ptr->next_timer = next_ptr->next_timer;
                }

                /** 
                 *  删除成功 
                 */
                status = EASYRTOS_OK;
                break;
            }

            prev_ptr = next_ptr;
            next_ptr = next_ptr->next_timer;

        }

        /** 
         *  退出临界区
         */
        CRITICAL_EXIT ();
     }
  return (status);
}

/**
 * @返回 EASYRTOS_OK 
 * @返回 EASYRTOS_ERR_PARAM 
 * @返回 EASYRTOS_ERR_CONTEXT 
 */
ERESULT eTimerDelay (uint32_t ticks)
{
    EASYRTOS_TCB *curr_tcb_ptr;
    EASYRTOS_TIMER timerCb;
    DELAY_TIMER timerData;
    CRITICAL_STORE;
    ERESULT status;

    /** 
     *  获取现在运行的任务TCB 
     */
    curr_tcb_ptr = eCurrentContext();

    /** 
     *  检查参数 
     */
    if (ticks == 0)
    {
        /** 
         *  错误返回
         */
        status = EASYRTOS_ERR_PARAM;
    }

    /** 
     *  检查是否运行在任务中
     */
    else if (curr_tcb_ptr == NULL)
    {
        /** 
         *  没有运行在任务中
         */
        status = EASYRTOS_ERR_CONTEXT;
    }

    else
    {
        /**
         *  进入临界区,保护任务队列
         */
        CRITICAL_ENTER ();

        /** 
         *  设置运行任务状态为Delay
         */
        curr_tcb_ptr->state = TASK_DELAY;

        /** 
         *  注册timer的回调函数. 
         */

        /** 
         *  填入回调函数
         */
        timerData.tcb_ptr = curr_tcb_ptr;

        timerCb.cb_func = eTimerDelayCallback;
        timerCb.cb_data = (POINTER)&timerData;
        timerCb.cb_ticks = ticks;

        /**
         *  存储超时回调(暂时我们不会使用他)
         */
        curr_tcb_ptr->delay_timo_cb = &timerCb;

        /** 
         *  注册定时器回调
         */
        if (eTimerRegister (&timerCb) != EASYRTOS_OK)
        {
            /** 
             *  退出临界区
             */
            CRITICAL_EXIT ();

            /** 
             *  timer注册未成功
             */
            status = EASYRTOS_ERR_TIMER;
        }
        else
        {
            /**
             *  退出临界区
             */
            CRITICAL_EXIT ();

            /** 
             *  timer注册成功
             */
            status = EASYRTOS_OK;

            /** 
             *  正在运行的任务被延迟,调度其他任务开始运行
             */
            easyRTOSSched (FALSE);
        }
    }

    return (status);
}

void eTimerTick (void)
{
  /**
   *  当系统启动的时候才运行
   */
  if (easyRTOSStarted)
  {
    /**
     *  增加系统tick计数
     */
    
    systemTicks++;

    /** 
     *  检测是否有函数需要回调. 
     */
    eTimerCallbacks ();
  }
}

static void eTimerCallbacks (void)
{
  EASYRTOS_TIMER *prev_ptr = NULL, *next_ptr = NULL, *saved_next_ptr = NULL;
  EASYRTOS_TIMER *callback_list_tail = NULL, *callback_list_head = NULL;

  /**
   *  搜索队列,减少所有定时器的计数,检测是否需要回调.
   */
  prev_ptr = next_ptr = timer_queue;
  while (next_ptr)
  {
    /** 
     *  将next timer保存在列表中 
     */
    saved_next_ptr = next_ptr->next_timer;

    /** 
     *  timer 入口是否到期?
     */
    if (--(next_ptr->cb_ticks) == 0)
    {
      /** 
       *  从列表中移除该入口
       */
      if (next_ptr == timer_queue)
      {
        /** 
         *  移除了列表的头
         */
        timer_queue = next_ptr->next_timer;
      }
      else
      {
        /** 
         *  移除了列表中或者列表尾
         */
        prev_ptr->next_timer = next_ptr->next_timer;
      }

      /*
       *  将该入口加入需要执行的回调函数列表.在我们遍历完整个列表之后
       *  再执行,因为有可能之后还有回调需要执行.

       *  重复利用EASYRTOS_TIMER结构体的 next_ptr 指针来保存回调列表.
       */
      if (callback_list_head == NULL)
      {
        /** 
         *  在列表中添加第一个回调.     
         */
        callback_list_head = callback_list_tail = next_ptr;
      }
      else
      {
        /** 
         *  在列表尾添加回调   
         */
        callback_list_tail->next_timer = next_ptr;
        callback_list_tail = callback_list_tail->next_timer;
      }

      /**
       *  标记该timer就是回调列表的最后一个
       */
      next_ptr->next_timer = NULL;

    }

    /**
     *  回调入口没有到期.在此退出,并减少其count值
     */
    else
    {
      prev_ptr = next_ptr;
    }

    next_ptr = saved_next_ptr;
  }

  /**
   *  检测是否有回调函数.
   */
  if (callback_list_head)
  {
      /**
       *  遍历整个回调列表.
       *  Walk the callback list 
       */
      next_ptr = callback_list_head;
      while (next_ptr)
      {
          /**
           *  保存列表中的 next timer,避免列表被修改.  
           */
          saved_next_ptr = next_ptr->next_timer;

          /** 
           *  调用回调
           */
          if (next_ptr->cb_func)
          {
              next_ptr->cb_func (next_ptr->cb_data);
          }

          /**
           *  找到回调列表中的下一个回调.
           */
          next_ptr = saved_next_ptr;
      }
  }
}

static void eTimerDelayCallback (POINTER cb_data)
{
    DELAY_TIMER *timer_data_ptr;
    CRITICAL_STORE;

    /** 
     *  获取DELAY_TIMER的结构体指针.
     */
    timer_data_ptr = (DELAY_TIMER *)cb_data;

    /** 
     *  检查参数是否为NULL
     */
    if (timer_data_ptr)
    {
        /** 
         *  进入临界区
         */
        CRITICAL_ENTER ();

        /** 
         *  将任务加入Ready队列
         */
        (void)tcbEnqueuePriority (&tcb_readyQ, timer_data_ptr->tcb_ptr);

        /** 
         *  退出临界区
         */
        CRITICAL_EXIT ();
    }
}

uint32_t eTimeGet(void)
{
    return (systemTicks);
}

void eTimeSet(uint32_t newTime)
{
    systemTicks = newTime;
}
