/**  
 * 作者: Roy.yu
 * 时间: 2016.10.28
 * 版本: V1.1
 * Licence: GNU GENERAL PUBLIC LICENSE
 */
/* 内部变量 */
static uint8_t *mem_heap;
static uint8_t *mem_brk;
/* 外部可调用函数 */
void memInit(uint16_t maxHeap);

/* 内部函数 */
void memInit(uint16_t maxHeap)
{
  mem_heap = (uint8_t*)0x000011;
  mem_brk  = (uint8_t*)(mem_heap+maxHeap);
}