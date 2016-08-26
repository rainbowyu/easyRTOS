;作者: Roy.yu
;时间: 2016.8.23
;版本: V0.1
;Licence: GNU GENERAL PUBLIC LICENSE

NAME EASYRTOSPORTASM
 SECTION .near_func.text:code

#include "vregs.inc"

;void archFirstThreadRestore (EASYRTOS_TCB *new_tcb_ptr)
  PUBLIC archFirstTaskRestore
archFirstTaskRestore:
  ; Parameter locations:
  ; new_tcb_ptr = X register (word-width)
  ; POINTER sp_save_ptr;
  ; (X) = *sp_save_ptr
  ldw X,(X)

  ; 切换栈指针到新任务的堆栈
  ldw SP,X

  ; ?b15 - ?b8 出栈
  POP ?b15
  POP ?b14
  POP ?b13
  POP ?b12
  POP ?b11
  POP ?b10
  POP ?b9
  POP ?b8

  ; RET PCH=SP++;PCL=SP++;
  ret

  ;void archContextSwitch (EASYRTOS_TCB *old_tcb_ptr, EASYRTOS_TCB *new_tcb_ptr)
    PUBLIC archContextSwitch
archContextSwitch:

  ; Parameter locations :
  ;   old_tcb_ptr = X register (word-width)
  ;   new_tcb_ptr = Y register (word-width)

  ; 参数传递位置:
  ;   old_tcb_ptr = X 寄存器 (16bit)
  ;   new_tcb_ptr = Y 寄存器 (16bit)
  ; STM8 CPU Registers:
  ; STM8的CPU寄存器

  ; 一些寄存器已经被自动保存了,我只需要额外保存那些不希望被破坏的寄存器.

  PUSH ?b8
  PUSH ?b9
  PUSH ?b10
  PUSH ?b11
  PUSH ?b12
  PUSH ?b13
  PUSH ?b14
  PUSH ?b15
  
  ; 从Y寄存器中复制出new_tcb_ptr,保存在?b0中
  ldw ?b0, Y

  ; 保存当前的栈指针到old_tcb_ptr中
  ldw Y, SP    ; 将当前的栈指针(被切换的任务)保存到Y寄存器中
  ldw (X), Y   ; 将栈指针保存在(X)的地址中,

  ldw X,?b0
  ldw X,(X)

  
  ldw SP,X

  ; IAR中需要保存和恢复 ?b8 to ?b15
  POP ?b15
  POP ?b14
  POP ?b13
  POP ?b12
  POP ?b11
  POP ?b10
  POP ?b9
  POP ?b8

  ret

  end
  