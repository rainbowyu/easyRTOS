 NAME EASYRTOSPORTASM
 SECTION .near_func.text:code

; Get definitions for virtual registers used by the compiler
#include "vregs.inc"

;void archFirstThreadRestore (EASYRTOS_TCB *new_tcb_ptr)
  PUBLIC archFirstTaskRestore
archFirstTaskRestore:
  ; Parameter locations:
  ; new_tcb_ptr = X register (word-width)
  ; POINTER sp_save_ptr;
  ; (X) = *sp_save_ptr
  ldw X,(X)

  ; Switch our current stack pointer to that of the new task.
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

  ; The "return address" left on the stack now will be the new;
  ; Task's entry point.;
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

  ; The basic scheme is that some registers will already be saved
  ; onto the stack if the caller wishes them not to be change.
  ; We only need to context-switch additional registers which the
  ; caller does not expect to be modified in a subroutine.
  ; 一些寄存器已经被自动保存了,我只需要额外保存那些不希望被破坏的寄存器.

  PUSH ?b8
  PUSH ?b9
  PUSH ?b10
  PUSH ?b11
  PUSH ?b12
  PUSH ?b13
  PUSH ?b14
  PUSH ?b15

  ; Take a copy of the new_tcb_ptr parameter from Y-reg in
  ; a temporary (?b0) register. We need to use Y briefly for SP
  ; access.
  ; 从Y寄存器中复制出new_tcb_ptr,保存在?b0中
  ldw ?b0, Y

  ; Store current stack pointer as first entry in old_tcb_ptr
  ; 保存当前的栈指针到old_tcb_ptr中
  ldw Y, SP    ; Move current stack pointer into Y register 将当前的栈指针(被切换的任务)保存到Y寄存器中
  ldw (X), Y   ; Store current stack pointer at first offset in TCB 将栈指针保存在(X)的地址中,


  ; At this point, all of the current Task's context has been saved
  ; so we no longer care about keeping the contents of any registers
  ; except ?b0 which contains our passed new_tcb_ptr parameter (a
  ; pointer to the TCB of the Task which we wish to switch in).
  ;
  ; Our stack frame now contains all registers which need to be
  ; preserved or context-switched. It also contains the return address
  ; which will be either a function called via an ISR (for preemptive
  ; switches) or a function called from Task context (for cooperative
  ; switches).
  ;
  ; In addition, the Task's stack pointer (after context-save) is
  ; stored in the task's TCB.

  ; We are now ready to restore the new Task's context. We switch
  ; our stack pointer to the new Task's stack pointer, and pop its
  ; context off the stack, before returning to the caller (the
  ; original caller when the new Task was last scheduled out).

  ; Get the new Task's stack pointer off the TCB (new_tcb_ptr).
  ; We kept a copy of new_tcb_ptr earlier in ?b0, copy it into X.
  ldw X,?b0

  ; Pull the first entry out of new_tcb_ptr (the new Task's
  ; stack pointer) into X register.
  ldw X,(X)

  ; Switch our current stack pointer to that of the new Task.
  ldw SP,X

  ; (IAR) We only save/restore ?b8 to ?b15
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
  