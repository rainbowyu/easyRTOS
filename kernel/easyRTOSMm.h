/**
 * 作者: Roy.yu
 * 时间: 2016.11.01
 * 版本: V1.1
 * Licence: GNU GENERAL PUBLIC LICENSE
 */
#ifndef __EASYRTOSMM_H__
#define __EASYRTOSMM_H__

#define BSIZE  1
#define HWSIZE 2
#define WSIZE  4
#define DSIZE  8

#define HDSIZE WSIZE
#define FTSIZE WSIZE

#define MAX(x,y) ((x)>(y)?(x):(y))

/*
 * 一个块由头+数据+尾组成
 * 头：3Byte+1Byte
 * 尾：3Byte+1Byte
 */

/* 对分配字节的大小的头和尾进行打包 格式为 分配大小|分配标志位 */
#define PACKHF(size,alloc) (((uint32_t)size<<8)|(uint32_t)(alloc))

/*
 * 获取p地址的值
 * 设置p地址的值为value
 */
#define GET(p) (*(uint32_t *)(p))
#define PUT(p,value) (*(uint32_t *)(p) = (value))

/*
 * 获取p地址的块大小
 * 获取p地址的块分配标志位
 */
#define GET_SIZE(p)  (uint32_t)((GET(p) & ~0xff)>>8)
#define GET_ALLOC(p) (GET(p) & 0x1)

/*
 * 获取地址为bp的块(BLOCK)的头的地址
 * 获取地址为bp的块(BLOCK)的尾的地址
 */
#define HDRP(bp) (uint8_t*)((uint8_t*)(bp) - HDSIZE)
#define FTRP(bp) (uint8_t*)((uint8_t*)(bp) + GET_SIZE(HDRP(bp)) - (HDSIZE+FTSIZE))

/*
 * 获取地址为bp的块下一个头的地址
 * 获取地址为bp的块前一个头的地址
 */
#define NEXT_BLKP(bp) (uint8_t*)((uint8_t*)(bp) + GET_SIZE((uint8_t*)(bp)-HDSIZE))
#define PREV_BLKP(bp) (uint8_t*)((uint8_t*)(bp) - GET_SIZE((uint8_t*)(bp)-HDSIZE-FTSIZE))

extern void eMemInit(uint8_t* heapAddr,uint16_t maxHeap);
extern uint8_t *eMalloc(uint16_t size);
extern void eFree(uint8_t *addr);
   
#endif
