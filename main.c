#include "easyRTOS.h"
#include "easyRTOSkernel.h"
#include "easyRTOSport.h"
#include "easyRTOSTimer.h"
#include "easyRTOSSem.h"

#define IDLE_STACK_SIZE_BYTES 256
#define TEST_STACK_SIZE_BYTES 512
#define SEM_STACK_SIZE_BYTES  512
NEAR static uint8_t idleTaskStack[IDLE_STACK_SIZE_BYTES];
NEAR static uint8_t testTaskStack[TEST_STACK_SIZE_BYTES];
NEAR static uint8_t semTaskStack[SEM_STACK_SIZE_BYTES];

EASYRTOS_TCB testTcb;
EASYRTOS_TCB semTcb;
EASYRTOS_SEM semSemMutex;
void testTaskFunc (uint32_t param);
void semTaskFunc (uint32_t param);

uint8_t flag=0;

int main( void )
{
  ERESULT status;
  CLK_HSIPrescalerConfig(CLK_PRESCALER_HSIDIV1);
  
  status = easyRTOSInit(&idleTaskStack[0], IDLE_STACK_SIZE_BYTES);
  
  if (status == EASYRTOS_OK)
  {
      /* Enable the system tick timer */
      archInitSystemTickTimer();

      /* Create an application task */
      status += eTaskCreat(&testTcb,
                   10, 
                   testTaskFunc, 
                   0,
                   &testTaskStack[0],
                   TEST_STACK_SIZE_BYTES,
                   "TEST",
                   1);
      status += eTaskCreat(&semTcb,
                   9, 
                   semTaskFunc, 
                   1,
                   &semTaskStack[0],
                   SEM_STACK_SIZE_BYTES,
                   "SEM",
                   2);
      semSemMutex = eSemCreateMutex();
      if (status == EASYRTOS_OK)
      {
         easyRTOSStart();
      }
  }
  return 0;
}

void testTaskFunc (uint32_t param)
{
  GPIO_Init(GPIOD, GPIO_PIN_2, GPIO_MODE_OUT_PP_HIGH_FAST);
  while (1)
  {
    if (eSemTake (&semSemMutex, 0) == EASYRTOS_OK)
    {
      if (!flag)GPIO_WriteReverse(GPIOD, GPIO_PIN_2);
      flag = 1;
      eSemGive(&semSemMutex);
    }
  }
}

void semTaskFunc (uint32_t param)
{
    while (1)
  {
    if (eSemTake(&semSemMutex, 0) == EASYRTOS_OK)
    {
      eTimerDelay(DELAY_S(param));
      if (eSemTake(&semSemMutex, 0) == EASYRTOS_OK)
      { 
        eTimerDelay(DELAY_S(param));
        if (eSemTake(&semSemMutex, 0) == EASYRTOS_OK)
        {
          eTimerDelay(DELAY_S(param));
          eSemGive(&semSemMutex);
        }
        eSemGive(&semSemMutex);
      }
      eSemGive(&semSemMutex);
      flag=0;
    }  
    eTimerDelay(DELAY_S(param));
  }
}