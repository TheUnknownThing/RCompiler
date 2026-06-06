	.text
	.option nopic

	.globl	_start
_start:
	call	main
	li	a7, 93
	ecall

	.globl	_Function_prelude_exit
_Function_prelude_exit:
	li	a7, 93
	ecall

	.globl	_Function_prelude_print
_Function_prelude_print:
	addi	sp, sp, -16
	sd	ra, 8(sp)
	call	_runtime_write_cstr
	ld	ra, 8(sp)
	addi	sp, sp, 16
	ret

	.globl	_Function_prelude_println
_Function_prelude_println:
	addi	sp, sp, -16
	sd	ra, 8(sp)
	call	_runtime_write_cstr
	la	a1, _runtime_newline
	li	a2, 1
	call	_runtime_write_stdout
	ld	ra, 8(sp)
	addi	sp, sp, 16
	ret

	.globl	_Function_prelude_printInt
_Function_prelude_printInt:
	addi	sp, sp, -16
	sd	ra, 8(sp)
	call	_runtime_write_i32
	ld	ra, 8(sp)
	addi	sp, sp, 16
	ret

	.globl	_Function_prelude_printlnInt
_Function_prelude_printlnInt:
	addi	sp, sp, -16
	sd	ra, 8(sp)
	call	_runtime_write_i32
	la	a1, _runtime_newline
	li	a2, 1
	call	_runtime_write_stdout
	ld	ra, 8(sp)
	addi	sp, sp, 16
	ret

	.globl	_Function_prelude_getInt
_Function_prelude_getInt:
	addi	sp, sp, -16
	sd	ra, 8(sp)
	call	_runtime_skip_ws
	la	t0, _runtime_input_pos
	ld	t1, 0(t0)
	la	t2, _runtime_input_len
	ld	t3, 0(t2)
	la	t4, _runtime_input_buf
	li	t5, 1
	bgeu	t1, t3, .Lget_int_done
	add	t6, t4, t1
	lbu	a0, 0(t6)
	li	a1, 45
	bne	a0, a1, .Lget_int_parse
	li	t5, -1
	addi	t1, t1, 1
.Lget_int_parse:
	li	a0, 0
	li	a2, 10
.Lget_int_loop:
	bgeu	t1, t3, .Lget_int_finish
	add	t6, t4, t1
	lbu	a1, 0(t6)
	li	a3, 48
	bltu	a1, a3, .Lget_int_finish
	li	a3, 57
	bltu	a3, a1, .Lget_int_finish
	addi	a1, a1, -48
	mul	a0, a0, a2
	add	a0, a0, a1
	addi	t1, t1, 1
	j	.Lget_int_loop
.Lget_int_finish:
	sd	t1, 0(t0)
	li	a1, 1
	beq	t5, a1, .Lget_int_done
	neg	a0, a0
.Lget_int_done:
	ld	ra, 8(sp)
	addi	sp, sp, 16
	ret

	.globl	_Function_prelude_getString
_Function_prelude_getString:
	addi	sp, sp, -16
	sd	ra, 8(sp)
	call	_runtime_skip_ws
	la	t0, _runtime_heap_ptr
	ld	a0, 0(t0)
	addi	t1, a0, 256
	sd	t1, 0(t0)
	mv	t2, a0
	la	t3, _runtime_input_pos
	ld	t4, 0(t3)
	la	t5, _runtime_input_len
	ld	t6, 0(t5)
	la	a1, _runtime_input_buf
	li	a2, 255
.Lget_string_loop:
	beqz	a2, .Lget_string_finish
	bgeu	t4, t6, .Lget_string_finish
	add	a3, a1, t4
	lbu	a4, 0(a3)
	li	a5, 32
	bleu	a4, a5, .Lget_string_finish
	sb	a4, 0(t2)
	addi	t2, t2, 1
	addi	t4, t4, 1
	addi	a2, a2, -1
	j	.Lget_string_loop
.Lget_string_finish:
	sb	zero, 0(t2)
	sd	t4, 0(t3)
	ld	ra, 8(sp)
	addi	sp, sp, 16
	ret

	.globl	_Function_prelude_builtin_memset
_Function_prelude_builtin_memset:
	mv	t0, a0
	andi	a1, a1, 255
.Lmemset_loop:
	beqz	a2, .Lmemset_done
	sb	a1, 0(t0)
	addi	t0, t0, 1
	addi	a2, a2, -1
	j	.Lmemset_loop
.Lmemset_done:
	ret

	.globl	_Function_prelude_builtin_memcpy
_Function_prelude_builtin_memcpy:
	mv	t0, a0
	mv	t1, a1
.Lmemcpy_loop:
	beqz	a2, .Lmemcpy_done
	lbu	t2, 0(t1)
	sb	t2, 0(t0)
	addi	t0, t0, 1
	addi	t1, t1, 1
	addi	a2, a2, -1
	j	.Lmemcpy_loop
.Lmemcpy_done:
	ret

_runtime_write_cstr:
	mv	a1, a0
	mv	t0, a0
.Lstrlen_loop:
	lbu	t1, 0(t0)
	beqz	t1, .Lstrlen_done
	addi	t0, t0, 1
	j	.Lstrlen_loop
.Lstrlen_done:
	sub	a2, t0, a1
	tail	_runtime_write_stdout

_runtime_write_i32:
	addi	sp, sp, -16
	sd	ra, 8(sp)
	sext.w	t0, a0
	la	t3, _runtime_int_buf_end
	li	t4, 10
	bnez	t0, .Lwrite_i32_nonzero
	li	t1, 48
	addi	t3, t3, -1
	sb	t1, 0(t3)
	j	.Lwrite_i32_emit
.Lwrite_i32_nonzero:
	bgez	t0, .Lwrite_i32_loop
	la	a1, _runtime_minus
	li	a2, 1
	call	_runtime_write_stdout
	neg	t0, t0
.Lwrite_i32_loop:
	remu	t1, t0, t4
	divu	t0, t0, t4
	addi	t1, t1, 48
	addi	t3, t3, -1
	sb	t1, 0(t3)
	bnez	t0, .Lwrite_i32_loop
.Lwrite_i32_emit:
	mv	a1, t3
	la	t5, _runtime_int_buf_end
	sub	a2, t5, t3
	call	_runtime_write_stdout
	ld	ra, 8(sp)
	addi	sp, sp, 16
	ret

_runtime_write_stdout:
	li	a0, 1
	li	a7, 64
	ecall
	ret

_runtime_skip_ws:
	addi	sp, sp, -16
	sd	ra, 8(sp)
	call	_runtime_read_input_once
	la	t0, _runtime_input_pos
	ld	t1, 0(t0)
	la	t2, _runtime_input_len
	ld	t3, 0(t2)
	la	t4, _runtime_input_buf
.Lskip_ws_loop:
	bgeu	t1, t3, .Lskip_ws_done
	add	t5, t4, t1
	lbu	t6, 0(t5)
	li	a0, 32
	bgtu	t6, a0, .Lskip_ws_done
	addi	t1, t1, 1
	j	.Lskip_ws_loop
.Lskip_ws_done:
	sd	t1, 0(t0)
	ld	ra, 8(sp)
	addi	sp, sp, 16
	ret

_runtime_read_input_once:
	la	t0, _runtime_input_ready
	ld	t1, 0(t0)
	bnez	t1, .Lread_input_done
	li	a0, 0
	la	a1, _runtime_input_buf
	li	a2, 1048576
	li	a7, 63
	ecall
	bltz	a0, .Lread_input_empty
	la	t2, _runtime_input_len
	sd	a0, 0(t2)
	j	.Lread_input_mark
.Lread_input_empty:
	la	t2, _runtime_input_len
	sd	zero, 0(t2)
.Lread_input_mark:
	li	t1, 1
	sd	t1, 0(t0)
.Lread_input_done:
	ret

	.section .rodata
_runtime_newline:
	.ascii	"\n"
_runtime_minus:
	.ascii	"-"

	.data
	.p2align	3
_runtime_heap_ptr:
	.dword	_runtime_heap

	.bss
	.p2align	3
_runtime_input_ready:
	.dword	0
_runtime_input_pos:
	.dword	0
_runtime_input_len:
	.dword	0
_runtime_int_buf:
	.skip	32
_runtime_int_buf_end:
_runtime_input_buf:
	.skip	1048576
_runtime_heap:
	.skip	1048576
