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

	mrc p15, 0, r0, c1, c0, 2
	orr r0, r0, #(1 << 20)
	orr r0, r0, #(1 << 21)
	orr r0, r0, #(1 << 22)
	orr r0, r0, #(1 << 23)
	mcr p15, 0, r0, c1, c0, 2
	vmrs r0, fpexc
	orr r0, r0, #(1 << 30)
	vmsr fpexc, r0

	bl __libc_start_main
	b .
