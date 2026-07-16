  .text

/* Saved-context frame, built by FJS_fiberSwitch on the fiber's own stack.
 * Offsets relative to the saved rsp:
 *
 * 0x38 return address (rip)  <- pushed by the `call`
 * 0x30 rbx
 * 0x28 rbp
 * ...  r12, r13, r14, r15
 * 0x04 mxcsr (4 bytes)
 * 0x00 x87 fcw (2 bytes + 2 padding)
 *
 * Total: 64 bytes. Saved rsp is 16-byte aligned (see below).
 */
  .globl FJS_fiberSwitch
  .type  FJS_fiberSwitch, @function
FJS_fiberSwitch:        /* void *FJS_fiberSwitch(void **SaveSlot, void *TargetSP, void *Arg) */
  /* 1: Park the current fiber - push callee-saved state onto its own stack */
  push    %rbx
  push    %rbp
  push    %r12
  push    %r13
  push    %r14
  push    %r15
  sub     $8, %rsp      /* allocate 8 bytes for MXCSR and FCW */
  stmxcsr 4(%rsp)       /* store MXCSR into the 4 bytes at offset 4 */
  fnstcw  (%rsp)        /* store x87 control word (FCW) at offset 0 */

  /* 2: Publish where its stack top ended up - the one-word context */
  mov     %rsp, (%rdi)  /* *SaveSlot = rsp */

  /* 3: Adopt the target fiber's stack. This is when the switch happens */
  mov     %rsi, %rsp    /* rsp = TargetSP */

  /* 4: Unpark the target - exact mirror of phase 1, then resume */
  fldcw   (%rsp)        /* load x87 control word (FCW) from offset 0 */
  ldmxcsr 4(%rsp)       /* load MXCSR from the 4 bytes at offset 4 */
  add     $8, %rsp      /* free the 8 bytes used by MXCSR and FCW */
  pop     %r15
  pop     %r14
  pop     %r13
  pop     %r12
  pop     %rbp
  pop     %rbx
  mov     %rdx, %rax    /* Arg becomes the target's return value */
  ret                   /* pops the return address -> resumes target */
  .size FJS_fiberSwitch, . - FJS_fiberSwitch

/* Entry point for brand-new fibers. Never called directly: its address is
 * planted as teh forget return address in a fabricated frame, so the first
 * switch into a new filer `ret`s here.
 *
 * Receives its inputs via the two "cargo" slots of that fabricated frame,
 * popped into registers by the restore phase of FJS_fiberSwitch:
 *
 *   r12  entry function pointer  void (*)(void *)
 *   r13  user artument           passed as arg 1 (rdi) 
 *
 * On entry rsp is 16-byte aligned (fabricated frames guarantee this), so the
 * `call` leaves the entry function with standard SysV alignment.
 *
 * the entry function must switch away, never return: this stack has no caller
 * to return to. The trailing ud2 turns any violation into an immediate SIGILL
 * instead of a jump into garbage.
 */
  .globl FJS_fiberTrampoline
  .type  FJS_fiberTrampoline, @function
FJS_fiberTrampoline:    /* [[noreturn]] void FJS_fiberTrampoline() - never call this one! */
  mov     %r13, %rdi    /* arg -> first parameter */
  call    *%r12         /* invoke the entry function */
  ud2                   /* entry returned: crash loudly (for now) */
  .size FJS_fiberTrampoline, . - FJS_fiberTrampoline

  .section .note.GNU-stack,"",@progbits
