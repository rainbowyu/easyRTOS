/**  
 * 作者: Roy.yu
 * 时间: 2016.10.28
 * 版本: V1.1
 * Licence: GNU GENERAL PUBLIC LICENSE
 */
#ifndef __EASYRTOSMM_H__
#define __EASYRTOSMM_H__

#define BSIZE  1
#define HWSIZE 2
#define WSIZE  4
#define DSIZE  8

#define HDSIZE 3
#define FTSIZE 3

#define MAX(x,y) ((x)>(y)?(x):(y))

/*
 * 一个块由头+数据+尾组成
 * 头：2.5Byte 尾0.5Byte
 */

/* 对分配字节的大小的头和尾进行打包 格式为 分配大小|分配标志位 */
#define PACKHF(size,alloc) ((size)|(alloc))

/* 
 * 获取p地址的数据 
 * 设置p地址的数据为value
 */
#define GET(p) (*(uint8_t *)(p))
#define PUT(p,value) (*(uint8_t *)(p) = (value))

/* 
 * 获取p地址的数据块大小 
 * 获取p地址的数据块分配标志位
 */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* 
 * 获取地址为bp的数据块的头的地址 
 * 获取地址为bp的数据块的尾的地址 
 */
#define HDRP(bp) ((uint8_t*)(bp) - HDSIZE)
#define FTRP(bp) ((uint8_t*)(bp) + GETSIZE(HDRP(bp)) - (HDSIZE+FTSIZE))

/* 
 * 获取地址为bp的数据块的头的地址 
 * 获取地址为bp的数据块的尾的地址 
 */
#define NEXT_BLKP(bp) ((uint8_t*)(bp) + GETSIZE((uint8_t)(bp)-))
#define PREV_BLKP(bp) ((uint8_t*)(bp) + GETSIZE(HDRP(bp)) - (HDSIZE+FTSIZE))

#endif