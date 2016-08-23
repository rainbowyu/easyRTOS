#include "easyRTOS.h"
#include "easyRTOSkernel.h"
#include "easyRTOSport.h"
#include "easyRTOSTimer.h"
#include "easyRTOSSem.h"
#include "easyRTOSQueue.h"

/* 包含 malloc() */
//#include "stdlib.h"

#define IDLE_STACK_SIZE_BYTES 256
#define TEST_STACK_SIZE_BYTES 512
#define QUEUE_STACK_SIZE_BYTES  512

NEAR static uint8_t idleTaskStack[IDLE_STACK_SIZE_BYTES];
NEAR static uint8_t testTaskStack[TEST_STACK_SIZE_BYTES];
NEAR static uint8_t queueTaskStack[QUEUE_STACK_SIZE_BYTES];

EASYRTOS_TCB testTcb;
EASYRTOS_TCB queueTcb;
EASYRTOS_QUEUE qQueue;
void testTaskFunc (uint32_t param);
void queueTaskFunc (uint32_t param);

typedef struct test{
  uint8_t delayTime1;
  uint8_t delayTime2;
}testStruct;

testStruct *a[2];

int main( void )
{
  ERESULT status;
  
  /* 设置CPU为内部时钟 16M*/
  CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV1);
  
  /* 系统初始化 */
  status = easyRTOSInit(&idleTaskStack[0], IDLE_STACK_SIZE_BYTES);
  
  if (status == EASYRTOS_OK)
  {
      /* 使能系统时钟 */
      archInitSystemTickTimer();

      /* 创建任务 */
      status += eTaskCreat(&testTcb,
                   9, 
                   testTaskFunc, 
                   0,
                   &testTaskStack[0],
                   TEST_STACK_SIZE_BYTES,
                   "TEST",
                   1);
      status += eTaskCreat(&queueTcb,
                   10, 
                   queueTaskFunc, 
                   1,
                   &queueTaskStack[0],
                   QUEUE_STACK_SIZE_BYTES,
                   "QUEUE",
                   2);
      
      /* 创建队列 */
      qQueue = eQueueCreate ( (void *)a, sizeof(testStruct *), 2);
      if (status == EASYRTOS_OK)
      {
        /* 启动系统 */
        easyRTOSStart();
      }
  }
  return 0;
}

void testTaskFunc (uint32_t param)
{
  testStruct *test1;
  GPIO_Init(GPIOD, GPIO_PIN_2, GPIO_MODE_OUT_PP_HIGH_FAST);
  while (1)
  {
    if (eQueueTake (&qQueue, 0, (void *)&test1) == EASYRTOS_OK)
    {
      GPIO_WriteReverse(GPIOD, GPIO_PIN_2);
      eTimerDelay(DELAY_S(test1->delayTime1+test1->delayTime2));
    }
  }
}

void queueTaskFunc (uint32_t param)
{
  testStruct test2;
  test2.delayTime1 = param;
  test2.delayTime2 = param;
  testStruct *temp =&test2;
  while (1)
  {
    temp = &test2;
    if (eQueueGive(&qQueue, 0, (void *)&temp) == EASYRTOS_OK)
    {
      
    }
  }
}