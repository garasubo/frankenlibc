__asm__ (".text\n\t"
	 ".global _start\n\t"
	 "_start:\n\t"
	 "	andq	$~15,%rsp\n\t"
	 "	movq	%rdx, %rdi\n\t"
	 "	movq	%rcx, %rsi\n\t"
	 "	movq	%rbx, %rdx\n\t"
	 "	call	__platform_init\n\t"
	 "1:	jmp 1b\n\t");
