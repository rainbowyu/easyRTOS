/**
 * 作者: Roy.yu
 * 时间: 2016.11.01
 * 版本: V1.1
 * Licence: GNU GENERAL PUBLIC LICENSE
 */
/* 内部变量 */
#include "easyRTOS.h"
#include "easyRTOSkernel.h"
#include "easyRTOSMm.h"
static uint8_t *mem_heap;
static uint8_t *mem_brk;

/* 外部可调用函数 */
void eMemInit(uint8_t* heapAddr,uint16_t maxHeap);
uint8_t *eMalloc(uint16_t size);
void eFree(uint8_t *addr);

/* 内部函数 */
static uint8_t *memBlockMerge(uint8_t *bp);
static void memBlockInit(void);
static uint8_t *findFitBp(uint16_t size);
static void placeBlock(uint8_t *bp,uint16_t size);
static uint8_t *memBlockMerge(uint8_t *bp);
/*
 *  分配器采用隐式链表管理内存分配空间，每个内存块由头（head）尾（foot）和中间内存组
 *  成，由下往上生长（地址增加，区别于栈）。堆的结束由堆最后的结束块标记，结束块表示为
 *  一个大小为0的已分配头（0/1）
 */

/*------------------------------------------------------------------------------*/
//分配器初始化 每个被分配的块需要4Byte头+4Byte尾 所以分配n Byte实际需要空间n+8Byte
void eMemInit(uint8_t* heapAddr,uint16_t maxHeap)
{
  mem_heap = heapAddr;
  mem_brk  = (uint8_t*)(mem_heap+maxHeap-4);
  memBlockInit();
}

//申请内存分配 找到合适的则分割空闲块，返回bp，若没有合适的则返回NULL
uint8_t *eMalloc(uint16_t size)
{
  uint8_t *bp;
  if (size==0)return NULL;
  if ((bp = findFitBp(size))!=NULL)
  {
    placeBlock(bp,size);
    return bp;
  }
  else
  {
    return NULL;
  }
}

//释放已分配的内存
void eFree(uint8_t *bp)
{
  uint16_t size = GET_SIZE(HDRP(bp));
  PUT(HDRP(bp),PACKHF(size,0));
  PUT(FTRP(bp),PACKHF(size,0));
  memBlockMerge(bp);
}

//块初始化
static void memBlockInit(void)
{
  uint8_t *bp = mem_heap+HDSIZE;
  uint32_t data = PACKHF((uint32_t)(mem_brk - mem_heap),0);
  static uint8_t *p;
  //初始化第一个free块
  PUT(HDRP(bp),data);
  PUT(FTRP(bp),data);
  //设置结束块标志
  p=NEXT_BLKP(bp);
  PUT(HDRP(p),PACKHF(0,1));
}

//搜寻合适大小的块 直到结束块标记返回NULL，找到则返回空闲块的bp
static uint8_t *findFitBp(uint16_t size)
{
  uint8_t *bp;
  //结束条件为遇到结束符号 找到后返回该bp
  for (bp = mem_heap+HDSIZE;!((GET_SIZE(HDRP(bp)) == 0) && (GET_ALLOC(HDRP(bp)) == 1)); bp = NEXT_BLKP(bp))
  {
    if ( (GET_SIZE(HDRP(bp)) >= (size+HDSIZE+FTSIZE)) && (!GET_ALLOC(HDRP(bp))) )
    {
      return bp;
    }
  }
  //未找到 返回NULL
  return NULL;
}

//分割块
static void placeBlock(uint8_t *bp,uint16_t size)
{
  uint16_t blockSize = GET_SIZE(HDRP(bp));
  uint16_t blockSizeNeed = size+HDSIZE+FTSIZE;
  PUT(HDRP(bp),PACKHF(blockSizeNeed,1));
  PUT(FTRP(bp),PACKHF(blockSizeNeed,1));
  PUT(HDRP(NEXT_BLKP(bp)),PACKHF(blockSize-blockSizeNeed,0));
  PUT(FTRP(NEXT_BLKP(bp)),PACKHF(blockSize-blockSizeNeed,0));
}

//空闲块合并
static uint8_t *memBlockMerge(uint8_t *bp)
{
  uint8_t prevAlloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  uint8_t nextAlloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

  //都被占用，则直接返回
  if (prevAlloc && nextAlloc)
  {
    return bp;
  }
  //前面被占用，后面块空闲，向后合并
  else if (prevAlloc && !nextAlloc)
  {
    PUT(HDRP(bp),PACKHF(GET_SIZE(HDRP(bp))+GET_SIZE(HDRP(NEXT_BLKP(bp))),0));
    PUT(FTRP(NEXT_BLKP(bp)),PACKHF(GET_SIZE(HDRP(bp))+GET_SIZE(HDRP(NEXT_BLKP(bp))),0));
    return bp;
  }
  //后面被占用，前面块空闲，向前合并
  else if (!prevAlloc && nextAlloc)
  {
    PUT(HDRP(PREV_BLKP(bp)),PACKHF(GET_SIZE(HDRP(bp))+GET_SIZE(HDRP(PREV_BLKP(bp))),0));
    PUT(FTRP(bp),PACKHF(GET_SIZE(HDRP(bp))+GET_SIZE(HDRP(PREV_BLKP(bp))),0));
    return PREV_BLKP(bp);
  }
  //两面都空闲，双向合并
  else if (!prevAlloc && !nextAlloc)
  {
    PUT(HDRP(PREV_BLKP(bp)),PACKHF(GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp))),0));
    PUT(FTRP(NEXT_BLKP(bp)),PACKHF(GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp))),0));
    return PREV_BLKP(bp);
  }
  else return NULL;
}

