	.attribute	4, 16
	.attribute	5, "rv64gc"
	.file	"builtin.c"
	.option	nopic
	.text
	.section	.rodata
.LC_print_str:
	.string	"%s"
.LC_println_str:
	.string	"%s\n"
.LC_print_int:
	.string	"%d"
.LC_println_int:
	.string	"%d\n"
	.text
	.align	1
	.globl	_Function_prelude_print
	.type	_Function_prelude_print, @function
_Function_prelude_print:
	addi	sp, sp, -16
	sd	ra, 8(sp)
	mv	a1, a0
	lla	a0, .LC_print_str
	call	printf
	ld	ra, 8(sp)
	addi	sp, sp, 16
	ret
	.size	_Function_prelude_print, .-_Function_prelude_print
	.align	1
	.globl	_Function_prelude_println
	.type	_Function_prelude_println, @function
_Function_prelude_println:
	addi	sp, sp, -16
	sd	ra, 8(sp)
	mv	a1, a0
	lla	a0, .LC_println_str
	call	printf
	ld	ra, 8(sp)
	addi	sp, sp, 16
	ret
	.size	_Function_prelude_println, .-_Function_prelude_println
	.align	1
	.globl	_Function_prelude_printInt
	.type	_Function_prelude_printInt, @function
_Function_prelude_printInt:
	addi	sp, sp, -16
	sd	ra, 8(sp)
	mv	a1, a0
	lla	a0, .LC_print_int
	call	printf
	ld	ra, 8(sp)
	addi	sp, sp, 16
	ret
	.size	_Function_prelude_printInt, .-_Function_prelude_printInt
	.align	1
	.globl	_Function_prelude_printlnInt
	.type	_Function_prelude_printlnInt, @function
_Function_prelude_printlnInt:
	addi	sp, sp, -16
	sd	ra, 8(sp)
	mv	a1, a0
	lla	a0, .LC_println_int
	call	printf
	ld	ra, 8(sp)
	addi	sp, sp, 16
	ret
	.size	_Function_prelude_printlnInt, .-_Function_prelude_printlnInt
	.align	1
	.globl	_Function_prelude_getString
	.type	_Function_prelude_getString, @function
_Function_prelude_getString:
	addi	sp, sp, -16
	sd	ra, 8(sp)
	li	a0, 256
	call	malloc
	sd	a0, 0(sp)
	mv	a1, a0
	lla	a0, .LC_print_str
	call	scanf
	ld	a0, 0(sp)
	ld	ra, 8(sp)
	addi	sp, sp, 16
	ret
	.size	_Function_prelude_getString, .-_Function_prelude_getString
	.align	1
	.globl	_Function_prelude_getInt
	.type	_Function_prelude_getInt, @function
_Function_prelude_getInt:
	addi	sp, sp, -16
	sd	ra, 8(sp)
	addi	a1, sp, 4
	lla	a0, .LC_print_int
	call	scanf
	lw	a0, 4(sp)
	ld	ra, 8(sp)
	addi	sp, sp, 16
	ret
	.size	_Function_prelude_getInt, .-_Function_prelude_getInt
	.align	1
	.globl	_Function_prelude_builtin_memset
	.type	_Function_prelude_builtin_memset, @function
_Function_prelude_builtin_memset:
	tail	memset
	.size	_Function_prelude_builtin_memset, .-_Function_prelude_builtin_memset
	.align	1
	.globl	_Function_prelude_builtin_memcpy
	.type	_Function_prelude_builtin_memcpy, @function
_Function_prelude_builtin_memcpy:
	tail	memcpy
	.size	_Function_prelude_builtin_memcpy, .-_Function_prelude_builtin_memcpy
	.align	1
	.globl	_Function_prelude_exit
	.type	_Function_prelude_exit, @function
_Function_prelude_exit:
	ret
	.size	_Function_prelude_exit, .-_Function_prelude_exit
