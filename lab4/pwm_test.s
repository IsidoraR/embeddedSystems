	.arch armv6
	.eabi_attribute 28, 1
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 6
	.eabi_attribute 34, 1
	.eabi_attribute 18, 4
	.file	"pwm_test.c"
	.section	.rodata
	.align	2
.LC0:
	.ascii	"Raspberry Pi wiringPi PWM test program\000"
	.text
	.align	2
	.global	main
	.syntax unified
	.arm
	.fpu vfp
	.type	main, %function
main:
	@ args = 0, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
	push	{fp, lr}
	add	fp, sp, #4
	sub	sp, sp, #8
	ldr	r0, .L10
	bl	puts
	bl	wiringPiSetup
	mov	r3, r0
	cmn	r3, #1
	bne	.L2
	mov	r0, #1
	bl	exit
.L2:
	mov	r3, #0
	str	r3, [fp, #-8]
	b	.L3
.L4:
	mov	r1, #1
	ldr	r0, [fp, #-8]
	bl	pinMode
	mov	r1, #0
	ldr	r0, [fp, #-8]
	bl	digitalWrite
	ldr	r3, [fp, #-8]
	add	r3, r3, #1
	str	r3, [fp, #-8]
.L3:
	ldr	r3, [fp, #-8]
	cmp	r3, #7
	ble	.L4
	mov	r1, #2
	mov	r0, #1
	bl	pinMode
.L9:
	mov	r3, #0
	str	r3, [fp, #-12]
	b	.L5
.L6:
	ldr	r1, [fp, #-12]
	mov	r0, #1
	bl	pwmWrite
	mov	r0, #1
	bl	delay
	ldr	r3, [fp, #-12]
	add	r3, r3, #1
	str	r3, [fp, #-12]
.L5:
	ldr	r3, [fp, #-12]
	cmp	r3, #1024
	blt	.L6
	ldr	r3, .L10+4
	str	r3, [fp, #-12]
	b	.L7
.L8:
	ldr	r1, [fp, #-12]
	mov	r0, #1
	bl	pwmWrite
	mov	r0, #1
	bl	delay
	ldr	r3, [fp, #-12]
	sub	r3, r3, #1
	str	r3, [fp, #-12]
.L7:
	ldr	r3, [fp, #-12]
	cmp	r3, #0
	bge	.L8
	b	.L9
.L11:
	.align	2
.L10:
	.word	.LC0
	.word	1023
	.size	main, .-main
	.ident	"GCC: (Raspbian 6.3.0-18+rpi1) 6.3.0 20170516"
	.section	.note.GNU-stack,"",%progbits
