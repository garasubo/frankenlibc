.global _Reset
_Reset:
	ldr sp, =stack_top
	ldr r0, =__bss_start
	ldr r1, =__bss_end
	mov r2, #0
	mov r3, #0
  bss_loop:
	stmia r0!, {r2-r3}
	cmp r0, r1
	blo bss_loop

	bl __libc_start_main
	b .
