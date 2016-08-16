#include "easyRTOS.h"
#include "easyRTOSkernel.h"
#include "easyRTOSport.h"
#include "easyRTOSTimer.h"
/* Local data */

/** Pointer to the head of the outstanding timers queue */
static EASYRTOS_TIMER *timer_queue = NULL;

/**
 *  number of easyRTOS system ticks
 *  easyRTOS系统滴答数量
 */
static uint32_t systemTicks = 0;

/* private function  */
static void eTimerCallbacks (void);
static void eTimerDelayCallback (POINTER cb_data);

/* public function  */
void eTimerCallbacks (void);
void eTimerTick (void);
ERESULT eTimerRegister (EASYRTOS_TIMER *timer_ptr);
ERESULT eTimerCancel (EASYRTOS_TIMER *timer_ptr);
ERESULT eTimerDelay (uint32_t ticks);
uint32_t eTimeGet(void);
void eTimeSet(uint32_t newTime);

/**
 * return EASYRTOS_OK
 * return EASYRTOS_ERR_PARAM
 */
ERESULT eTimerRegister (EASYRTOS_TIMER *timer_ptr)
{
    ERESULT status;
    CRITICAL_STORE;

    /**
     *  检查参数
     *  Parameter check   
     */
    if ((timer_ptr == NULL) || (timer_ptr->cb_func == NULL)
        || (timer_ptr->cb_ticks == 0))
    {
        /** 
         *  错误返回
         *  Return error 
         */
        status = EASYRTOS_ERR_PARAM;
    }
    else
    {
        /** 
         *  进入临界区
         *  Protect the list 
         */
        CRITICAL_ENTER ();

        /*
         *  timer加入列表
         *
         *  列表是没有顺序的,所有的timer都从列表的头插入.每个系统心跳,该列表
         *  中的所有计数将会减少1,当count减到0,则timer的回掉函数会被启动.
         *
         *  Enqueue in the list of timers.
         *  
         *  The list is not ordered, all timers are inserted at the start
         *  of the list. On each system tick increment the list is walked
         *  and the remaining ticks count for that timer is decremented.
         *  Once the remaining ticks reaches zero, the timer callback is
         *  made.
         *
         */
        if (timer_queue == NULL)
        {
            /** 
             *  List is empty, insert new head 
             *  列表为空,插入头列表的头
             */
            timer_ptr->next_timer = NULL;
            timer_queue = timer_ptr;
        }
        else
        {
            /** \
             *  列表已经有了timer,在列表头插入新的timer
             *  List has at least one entry, enqueue new timer before 
             */
            timer_ptr->next_timer = timer_queue;
            timer_queue = timer_ptr;
        }

        /*
         *  退出临界区
         *  End of list protection 
         */
        CRITICAL_EXIT ();

        /** 
         *  注册成功 
         *  Successful 
         */
        status = EASYRTOS_OK;
    }

    return (status);
}


/**
 * return EASYRTOS_OK
 * return EASYRTOS_ERR_PARAM
 * return EASYRTOS_ERR_NOT_FOUND
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
         *  Return error 
         */
        status = EASYRTOS_ERR_PARAM;
    }
    else
    {
        /**
         *  进入临界区
         *  Protect the list 
         */
        CRITICAL_ENTER ();

        /** 
         *  搜寻列表,找到相关的timer
         *  Walk the list to find the relevant timer 
         */
        prev_ptr = next_ptr = timer_queue;
        while (next_ptr)
        {
            /** 
             *  是否是我们寻找的timer
             *  Is this entry the one we're looking for? 
             */
            if (next_ptr == timer_ptr)
            {
                if (next_ptr == timer_queue)
                {
                    /** 
                     *  移除的是列表头
                     *  We're removing the list head  
                     */
                    timer_queue = next_ptr->next_timer;
                }
                else
                {
                    /** 
                     *  移除的是列表中或者列表尾
                     *  We're removing a mid or tail TCB   
                     */
                    prev_ptr->next_timer = next_ptr->next_timer;
                }

                /** 
                 *  删除成功
                 *  Successful   
                 */
                status = EASYRTOS_OK;
                break;
            }

            prev_ptr = next_ptr;
            next_ptr = next_ptr->next_timer;

        }

        /** 
         *  退出临界区
         *  End of list protection 
         */
        CRITICAL_EXIT ();
     }
  return (status);
}

/**
 * @return EASYRTOS_OK Successful delay
 * @return EASYRTOS_ERR_PARAM Bad parameter (ticks must be non-zero)
 * @return EASYRTOS_ERR_CONTEXT Not called from task context
 */
ERESULT eTimerDelay (uint32_t ticks)
{
    EASYRTOS_TCB *curr_tcb_ptr;
    EASYRTOS_TIMER timerCb;
    DELAY_TIMER timerData;
    CRITICAL_STORE;
    ERESULT status;

    /** 
     *  获取现在运行的TCB
     *  Get the current TCB  
     */
    curr_tcb_ptr = eCurrentContext();

    /** 
     *  检查参数
     *  Parameter check 
     */
    if (ticks == 0)
    {
        /** 
         *  错误返回
         *  Return error 
         */
        status = EASYRTOS_ERR_PARAM;
    }

    /** 
     *  检查是否运行在任务中
     *  Check we are actually in task context 
     */
    else if (curr_tcb_ptr == NULL)
    {
        /** 
         *  没有运行在任务中
         *  Not currently in task context, can't delay 
         */
        status = EASYRTOS_ERR_CONTEXT;
    }

    else
    {
        /**
         *  进入临界区,保护任务队列
         *  Protect the system queues 
         */
        CRITICAL_ENTER ();

        /** 
         *  设置运行任务状态为Delay
         *  Set delay status for the current task */
        curr_tcb_ptr->state = TASK_DELAY;

        /** 
         *  注册timer的回调函数.
         *  Register the timer callback 
         */

        /** 
         *  填入回调函数
         *  Fill out the callback to wake us up 
         */
        timerData.tcb_ptr = curr_tcb_ptr;

        /* Fill out the timer callback request structure */
        timerCb.cb_func = eTimerDelayCallback;
        timerCb.cb_data = (POINTER)&timerData;
        timerCb.cb_ticks = ticks;

        /**
         *  存储超时回调(即使我们不会使用他)
         *  Store the timeout callback details, though we don't use it 
         */
        curr_tcb_ptr->delay_timo_cb = &timerCb;

        /** 
         *  注册定时器回调
         *  Register the callback 
         */
        if (eTimerRegister (&timerCb) != EASYRTOS_OK)
        {
            /** 
             *  退出临界区
             *  Exit critical region 
             */
            CRITICAL_EXIT ();

            /** 
             *  timer注册未成功
             *  Timer registration didn't work, won't get a callback 
             */
            status = EASYRTOS_ERR_TIMER;
        }
        else
        {
            /**
             *  退出临界区
             *  Exit critical region 
             */
            CRITICAL_EXIT ();

            /** 
             *  timer注册成功
             *  Successful timer registration 
             */
            status = EASYRTOS_OK;

            /** 
             *  正在运行的任务被延迟,调度其他任务开始运行
             *  Current task should now delay, schedule in another 
             */
            easyRTOSSched (FALSE);
        }
    }

    return (status);
}

void eTimerTick (void)
{
  /**
   *  Only do anything if the OS is started
   *  当系统启动的时候才运行
   */
  if (easyRTOSStarted)
  {
    /**
     *  Increment the system tick count 
     *  增加系统tick计数
     */
    
    systemTicks++;

    /** 
     *  Check for any callbacks that are due 
     *  检测是否有函数需要回调.
     */
    eTimerCallbacks ();
  }
}

static void eTimerCallbacks (void)
{
  EASYRTOS_TIMER *prev_ptr = NULL, *next_ptr = NULL, *saved_next_ptr = NULL;
  EASYRTOS_TIMER *callback_list_tail = NULL, *callback_list_head = NULL;

  /*
   * Walk the list decrementing each timer's remaining ticks count and
   * looking for due callbacks.
   * 搜索队列,减少所有定时器的计数,检测是否需要回调.
   */
  prev_ptr = next_ptr = timer_queue;
  while (next_ptr)
  {
    /** 
     *  Save the next timer in the list (we adjust next_ptr for callbacks) 
     *  将next timer保存在列表中
     */
    saved_next_ptr = next_ptr->next_timer;

    /** 
     *  Is this entry due? 
     *  timer 入口是否到期?
     */
    if (--(next_ptr->cb_ticks) == 0)
    {
      /** 
       *  Remove the entry from the timer list 
       *  从列表中移除该入口
       */
      if (next_ptr == timer_queue)
      {
        /** 
         *  We're removing the list head 
         *  移除了列表的头
         */
        timer_queue = next_ptr->next_timer;
      }
      else
      {
        /** 
         *  We're removing a mid or tail timer 
         *  移除了列表中或者列表尾
         */
        prev_ptr->next_timer = next_ptr->next_timer;
      }

      /*
       *  Add this timer to the list of callbacks to run later when
       *  we've finished walking the list (we shouldn't call callbacks
       *  now in case they want to register new timers and hence walk
       *  the timer list.)
       *  将该入口加入需要执行的回调函数列表.在我们遍历完整个列表之后
       *  再执行,因为有可能之后还有回调需要执行.
       *
       *  We reuse the EASYRTOS_TIMER structure's next_ptr to maintain the
       *  callback list.
       *  重复利用EASYRTOS_TIMER结构体的 next_ptr 指针来保存回调列表.
       */
      if (callback_list_head == NULL)
      {
        /** 
         *  First callback request in the list 
         *  在列表中添加第一个回调.
         */
        callback_list_head = callback_list_tail = next_ptr;
      }
      else
      {
        /** 
         *  Add callback request to the list tail 
         *  在列表尾添加回调
         */
        callback_list_tail->next_timer = next_ptr;
        callback_list_tail = callback_list_tail->next_timer;
      }

      /**
       *  Mark this timer as the end of the callback list 
       *  标记该timer就是回调列表的最后一个
       */
      next_ptr->next_timer = NULL;

      /* Do not update prev_ptr, we have just removed this one */
    }

    /**
     *  Entry is not due, leave it in there with its count decremented 
     *  回调入口没有到期.在此退出,并减少其count值
     */
    else
    {
      /**
       * Update prev_ptr to this entry. We will need it if we want
       * to remove a mid or tail timer.
       */
      prev_ptr = next_ptr;
    }

    /* Move on to the next in the list */
    next_ptr = saved_next_ptr;
  }

  /**
   *  Check if any callbacks were due. 
   *  检测是否有回调函数.
   */
  if (callback_list_head)
  {
      /**
       *  Walk the callback list 
       *  遍历整个回调列表.
       */
      next_ptr = callback_list_head;
      while (next_ptr)
      {
          /*
           *  Save the next timer in the list (in case the callback
           *  modifies the list by registering again.
           *  保存列表中的 next timer,避免列表被修改.
           */
          saved_next_ptr = next_ptr->next_timer;

          /** 
           *  Call the registered callback 
           *  调用回调
           */
          if (next_ptr->cb_func)
          {
              next_ptr->cb_func (next_ptr->cb_data);
          }

          /**
           *  Move on to the next callback in the list 
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
     *  Get the DELAY_TIMER structure pointer 
     */
    timer_data_ptr = (DELAY_TIMER *)cb_data;

    /** 
     *  检查参数是否为NULL
     *  Check parameter is valid 
     */
    if (timer_data_ptr)
    {
        /* Enter critical region */
        CRITICAL_ENTER ();

        /* Put the task on the ready queue */
        (void)tcbEnqueuePriority (&tcb_readyQ, timer_data_ptr->tcb_ptr);

        /* Exit critical region */
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
