/**  
 * 作者: Roy.yu
 * 时间: 2016.8.23
 * 版本: V0.1
 * Licence: GNU GENERAL PUBLIC LICENSE
 */
#include "easyRTOS.h"
#include "easyRTOSkernel.h"
#include "easyRTOSport.h"
#include "easyRTOSTimer.h"
#include "stm8s_tim3.h"

/* 全局函数 */
void archTaskContextInit (EASYRTOS_TCB *tcb_ptr, void *stack_top, void (*entry_point)(uint32_t), uint32_t entryParam);
void archInitSystemTickTimer ( void );
/* end */

/* 私有函数 */
static void taskShell (void);
/* end */

/**
 * 功能: 任务入口函数,将tcb.entry_point参数传递给任务函数,
 * 并设置当前运行上下文,使能中断.
 *
 * 参数:
 * 输入:                         输出:
 * 无                            无.
 *
 * 返回: void
 * 
 * 调用的函数:
 * eCurrentContext();
 * rim();
 * curr_tcb->entry_point(curr_tcb->entryParam);
 */
static void taskShell (void)
{
    EASYRTOS_TCB *curr_tcb;

    curr_tcb = eCurrentContext();

    /* 使能中断 */
    rim();

    /* 任务函数入口 */
    if (curr_tcb && curr_tcb->entry_point)
    {
        curr_tcb->entry_point(curr_tcb->entryParam);
    }
}

/**
 * 功能: 初始化各个任务的堆栈,确定虚拟寄存器?b8-?b15的位置,
 * 以及函数入口地址,供汇编中RET使用.
 *
 * 参数:
 * 输入:                         输出:
 * 无                            无.
 *
 * 返回: void
 * 
 * 调用的函数:
 * eCurrentContext();
 * rim();
 * curr_tcb->entry_point(curr_tcb->entryParam);
 */
void archTaskContextInit (EASYRTOS_TCB *tcb_ptr, void *stack_top, void (*entry_point)(uint32_t), uint32_t entryParam)
{
    uint8_t *stack_ptr;
    stack_ptr = (uint8_t *)stack_top;
    /**
     *  任务恢复程序会执行 RET 指令.该指令需要在堆栈中找到被切换的任务的程序地址.
     *  当一个任务第一次运行时,会返回任务入口.我们将任务入口存储在堆栈中,同时RET
     *  也将在堆栈中寻找任务程序返回地址.

     *  我们用 taskShell() 函数启动所有的任务,所以实际上我们将taskShell()的
     *  地址存储在这里.

     *  从上到下使用堆栈.
     */
    *stack_ptr-- = (uint8_t)((uint16_t)taskShell & 0xFF);
    *stack_ptr-- = (uint8_t)(((uint16_t)taskShell >> 8) & 0xFF);

    /* 使用IAR的时候会有一些虚拟寄存器,初始化这些虚拟寄存器的位置 */
    *stack_ptr-- = 0;    // ?b8
    *stack_ptr-- = 0;    // ?b9
    *stack_ptr-- = 0;    // ?b10
    *stack_ptr-- = 0;    // ?b11
    *stack_ptr-- = 0;    // ?b12
    *stack_ptr-- = 0;    // ?b13
    *stack_ptr-- = 0;    // ?b14
    *stack_ptr-- = 0;    // ?b15

    /**
     *  与任务上下文有关的数据都已初始化完毕.剩下的就是在TCB中保存栈指针,供调度器
     *  使用.
     */
    tcb_ptr->sp_save_ptr = stack_ptr;
}

/**
 * 功能: 初始化系统心跳使用的定时器,这里使用的是TIM3,也可以使用其他定时器
 * 频率由SYSTEM_TICKS_HZ调整,该频率即为系统自动切换任务的时间.
 *
 * 参数:
 * 输入:                         输出:
 * 无                            无.
 *
 * 返回: void
 * 
 * 调用的函数:
 * TIM3_DeInit();
 * TIM3_TimeBaseInit(TIM3_PRESCALER_16, (uint16_t)((uint32_t)1000000/SYSTEM_TICKS_HZ));
 * TIM3_ITConfig(TIM3_IT_UPDATE, ENABLE);
 * TIM3_Cmd(ENABLE);
 */
void archInitSystemTickTimer ( void )
{
    /* 初始化TIM3*/
    TIM3_DeInit();

    /* 配置系统心跳 */
    TIM3_TimeBaseInit(TIM3_PRESCALER_16, (uint16_t)((uint32_t)1000000/SYSTEM_TICKS_HZ));

    TIM3_ITConfig(TIM3_IT_UPDATE, ENABLE);

    /* 使能TIM3 */
    TIM3_Cmd(ENABLE);
}

/**
 * 功能: 系统心跳时钟中断程序,将更新所有定时器的count,调用所有要调用
 * 的定时器回调,并在退出的时候调用调度器.
 *
 * 参数:
 * 输入:                         输出:
 * 无                            无.
 *
 * 返回: void
 * 
 * 调用的函数:
 * eIntEnter ();
 * eTimerTick();
 * eIntExit (TRUE);
 */
/* IAR中中断向量 */
#pragma vector = ITC_IRQ_TIM3_OVF + 2
__interrupt  void TIM3_SystemTickISR (void)
{
    eIntEnter ();
    
    eTimerTick();

    TIM3->SR1 = (uint8_t)(~(uint8_t)TIM3_IT_UPDATE);
    
    eIntExit (TRUE);
}
