@
@	dispatch.s
@
	.syntax unified
	.cpu cortex-m4
	.fpu fpv4-sp-d16
	.thumb
 @	IMPORTS 
 	.global	DUMP
	.global	DUMPD
	.global	DUMPH
	.global current
	.global upcoming
 @	EXPORTS
	.global vect_SVCall
	.global vect_PendSV
  

@	void vect_SVCall ( void)
    .section  .text,"ax",%progbits
	.type  vect_SVCall, %function

vect_SVCall:
	ldr	r1, =L00
	b vect_PendSV1

@
@	void vect_PendSV ( void)
    .section  .text,"ax",%progbits
	.type  vect_PendSV, %function

vect_PendSV:
	ldr r1, =L01

vect_PendSV1:
	mrs r0, msp
	vstmdb r0!, {s16-s31} @ save floating point registers
	mov r2, lr
	mrs r3, basepri
	stmdb r0!, {r2-r11} @ save LR, BASEPRI and R4 to R11

	ldr r7, =current
	ldr	r8, [r7]
	str r0, [r8] @ save thread context
	
	b L000a
	
@ Debug printout
	mov r0, r1
	bl		DUMP
	ldr	r0, =L99
	bl		DUMP

	ldr	r0, =L03a
	bl		DUMP
	ldr	r0, [r8, #4] @ Thread ID
	bl		DUMPD
	ldr	r0, =L03b
	bl		DUMP
	ldr	r0, [r8] @ Thread context
	bl		DUMPH
	ldr	r0, =L03c
	bl		DUMP
	ldr	r0, [r8] 
	ldr	r0, [r0] @ EXC_RETURN value
	bl		DUMPH
	ldr	r0, =L99
	bl		DUMP

L000a:
	ldr	r8, =upcoming
	ldr	r8, [r8]
	str r8, [r7] @ set current = upcoming

	b L000b
	
	ldr	r0, =L04a
	bl		DUMP
	ldr	r0, [r8, #4] @ Thread ID
	bl		DUMPD
	ldr	r0, =L04b
	bl		DUMP
	ldr	r0, [r8] @ Thread context
	bl		DUMPH
	ldr	r0, =L03c
	bl		DUMP
	ldr	r0, [r8] 
	ldr	r0, [r0] @ EXC_RETURN value
	bl		DUMPH
	ldr	r0, =L99
	bl		DUMP

L000b:
@ Clear Pend_SV exception	
	ldr	r2,=0xE000ED00
	ldr	r3, [r2, #4]	@ SCB->ICSR
	orr	r3, r3, #(1<<27)	@ SCB_ICSR_PENDSVCLR_Msk
	str	r3, [r2, #4]	@ SCB->ICSR
	nop

	ldr r0, [r8] @ save thread context
	ldmia r0!, {r2-r11} @ load LR, BASEPRI and R4 to R11
	msr basepri, r3
	mov lr, r2
	vldmia r0!, {s16-s31} @ load floating point registers
	msr msp, r0
	bx lr

	.size  vect_PendSV, .-vect_PendSV
	
	.data
	.align	2
L00:
	.ascii	"SVCall exception\000"
L01:
	.ascii	"PendSV exception\000"
L02:
	.ascii	", EXC_RETURN = \000"
L03a:
	.ascii	"  current thread: ID = \000"
L03b:
	.ascii	", SP = \000"
L03c:
	.ascii	", *SP = \000"
L04a:
	.ascii	"  next thread: ID = \000"
L04b:
	.ascii	", SP = \000"
L04c:
	.ascii	", *SP = \000"
L99:
	.ascii	"\n\r\000"

	

