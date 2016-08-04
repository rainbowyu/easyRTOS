; void archFirstThreadRestore (ATOM_TCB *new_tcb_ptr)
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

    ; The "return address" left on the stack now will be the new
    ; thread's entry point.
    ; RET PCH=SP++;PCL=SP++;
    ret

    end
