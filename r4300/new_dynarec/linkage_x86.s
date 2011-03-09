/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - linkage_x86.s                                           *
 *   Copyright (C) 2009-2010 Ari64                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
	.file	"linkage_x86.s"
	.bss
	.align 4
.globl rdram
rdram = 0x80000000
/*rdram:
	.space	8388608
	.type	rdram, %object
	.size	rdram, 8388608
*/
	.section	.rodata
	.text
.globl dyna_linker
	.type	dyna_linker, @function
dyna_linker:
	leal	0x80000000(%eax), %ecx
	movl	$2048, %edx
	shrl	$12, %ecx
	cmpl	%edx, %ecx
	cmova	%edx, %ecx
	movl	jump_in(,%ecx,4), %edx
.L1:
	test	%edx, %edx
	je	.L3
	mov	(%edx), %edi
	mov	4(%edx), %ebp
	xor	%eax, %edi
	or	%ebp, %edi
	je	.L2
	movl	12(%edx), %edx
	jmp	.L1
.L2:
	mov	(%ebx), %edi
	mov	%esi, %ebp
	lea	4(%ebx,%edi,1), %esi
	mov	%eax, %edi
	pusha
	call	add_link
	popa
	mov	8(%edx), %edi
	mov	%ebp, %esi
	lea	-4(%edi), %ebp
	subl	%ebx, %ebp
	movl	%ebp, (%ebx)
	jmp	*%edi
.L3:
	movl	jump_dirty(,%ecx,4), %edx
.L4:
	testl	%edx, %edx
	je	.L6
	movl	(%edx), %edi
	cmpl	%edi, %eax
	je	.L5
	movl	12(%edx), %edx
	jmp	.L4
.L5:
	mov	8(%edx), %edi
	jmp	*%edi
.L6:
	mov	%eax, %edi
	pusha
	call	new_recompile_block
	popa
	jmp	dyna_linker
	.size	dyna_linker, .-dyna_linker

.globl verify_code
	.type	verify_code, @function
verify_code:
	mov	%esi, cycle_count
	test	$7, %ecx
	je	.L7
	mov	-4(%eax,%ecx,1), %esi
	mov	-4(%ebx,%ecx,1), %edi
	add	$-4, %ecx
	xor	%esi, %edi
	jne	.L8
.L7:
	mov	-4(%eax,%ecx,1), %edx
	mov	-4(%ebx,%ecx,1), %ebp
	mov	-8(%eax,%ecx,1), %esi
	xor	%edx, %ebp
	mov	-8(%ebx,%ecx,1), %edi
	jne	.L8
	xor	%esi, %edi
	jne	.L8
	add	$-8, %ecx
	jne	.L7
	mov	cycle_count, %esi /* CCREG */
	ret
.L8:
	add	$4, %esp /* pop return address, we're not returning */
	call	remove_hash
	call	new_recompile_block
	call	get_addr
	mov	cycle_count, %esi
	add	$4, %esp /* pop virtual address */
	jmp	*%eax
	.size	verify_code, .-verify_code

.globl fp_exception
	.type	fp_exception, @function
fp_exception:
	mov	$0x1000002c, %edx
.fpe:
	mov	reg_cop0+48, %ebx
	or	$2, %ebx
	mov	%ebx, reg_cop0+48 /* Status */
	mov	%edx, reg_cop0+52 /* Cause */
	mov	%eax, reg_cop0+56 /* EPC */
	push	%esi
	push    $0x80000180
	call	get_addr_ht
	pop	%esi
	pop	%esi
	jmp	*%eax
	.size	fp_exception, .-fp_exception

.globl fp_exception_ds
	.type	fp_exception_ds, @function
fp_exception_ds:
	mov	$0x9000002c, %edx /* Set high bit if delay slot */
	jmp	.fpe
	.size	fp_exception_ds, .-fp_exception_ds

.globl jump_syscall
	.type	jump_syscall, @function
jump_syscall:
	mov	$0x20, %edx
	mov	reg_cop0+48, %ebx
	or	$2, %ebx
	mov	%ebx, reg_cop0+48 /* Status */
	mov	%edx, reg_cop0+52 /* Cause */
	mov	%eax, reg_cop0+56 /* EPC */
	push	%esi
	push    $0x80000180
	call	get_addr_ht
	pop	%esi
	pop	%esi
	jmp	*%eax
	.size	jump_syscall, .-jump_syscall

.globl cc_interrupt
	.type	cc_interrupt, @function
cc_interrupt:
	add	last_count, %esi
	add	$-28, %esp /* Align stack */
	mov	%esi, reg_cop0+36 /* Count */
	call	gen_interupt
	mov	reg_cop0+36, %esi
	mov	next_interupt, %eax
	mov	pending_exception, %ebx
	mov	stop, %ecx
	add	$28, %esp
	mov	%eax, last_count
	sub	%eax, %esi
	test	%ecx, %ecx
	jne	.L10
	test	%ebx, %ebx
	jne	.L9
	ret
.L9:
	mov	pcaddr, %edi
	mov	%esi, cycle_count /* CCREG */
	push	%edi
	call	get_addr_ht
	mov	cycle_count, %esi
	add	$8, %esp
	jmp	*%eax
.L10:
	add	$16, %esp /* pop stack */
	pop	%edi /* restore edi */
	pop	%esi /* restore esi */
	pop	%ebx /* restore ebx */
	pop	%ebp /* restore ebp */
	ret	     /* exit dynarec */
	.size	cc_interrupt, .-cc_interrupt

.globl new_dyna_start
	.type	new_dyna_start, @function
new_dyna_start:
	push	%ebp
	push	%ebx
	push	%esi
	push	%edi
	push	$0xa4000040
	call	new_recompile_block
	add	$-8, %esp /* align stack */
	movl	next_interupt, %edi
	movl	reg_cop0+36, %esi
	movl	%edi, last_count
	subl	%edi, %esi
	jmp	0x70000000
	.size	new_dyna_start, .-new_dyna_start

.globl jump_vaddr
	.type	jump_vaddr, @function
jump_vaddr:
  /* Check hash table */
	mov	%eax, %edi
	shr	$16, %eax
	xor	%edi, %eax
	movzwl	%ax, %eax
	shl	$4, %eax
	cmp	hash_table(%eax), %edi
	jne	.L12
.L11:
	mov	hash_table+4(%eax), %edi
	jmp	*%edi
.L12:
	cmp	hash_table+8(%eax), %edi
	lea	8(%eax), %eax
	je	.L11
  /* No hit on hash table, call compiler */
	push	%edi
	mov	%esi, cycle_count /* CCREG */
	call	get_addr
	mov	cycle_count, %esi
	add	$4, %esp
	jmp	*%eax
	.size	jump_vaddr, .-jump_vaddr

.globl jump_eret
	.type	jump_eret, @function
jump_eret:
	mov	reg_cop0+48, %ebx /* Status */
	and	$0xFFFFFFFD, %ebx
	mov	%ebx, reg_cop0+48 /* Status */
	mov	reg_cop0+56, %eax /* EPC */
	push	%esi
	mov	$248, %ebx
	xor	%edi, %edi
.L13:
	mov	reg(%ebx), %ecx
	mov	reg+4(%ebx), %edx
	sar	$31, %ecx
	xor	%ecx, %edx
	neg	%edx
	adc	%edi, %edi
	sub	$8, %ebx
	jne	.L13
	mov	hi(%ebx), %ecx
	mov	hi+4(%ebx), %edx
	sar	$31, %ecx
	xor	%ecx, %edx
	jne	.L14
	mov	lo(%ebx), %ecx
	mov	lo+4(%ebx), %edx
	sar	$31, %ecx
	xor	%ecx, %edx
.L14:
	neg	%edx
	adc	%edi, %edi
	push	%edi
	push	%eax
	call	get_addr_32
	pop	%esi
	pop	%esi
	pop	%esi
	jmp	*%eax
	.size	jump_eret, .-jump_eret

.globl write_rdram_new
	.type	write_rdram_new, @function
write_rdram_new:
	mov	address, %edi
	mov	word, %ecx
	mov	%ecx, rdram-0x80000000(%edi)
	jmp	.L15
	.size	write_rdram_new, .-write_rdram_new

.globl write_rdramb_new
	.type	write_rdramb_new, @function
write_rdramb_new:
	mov	address, %edi
	xor	$3, %edi
	movb	byte, %cl
	movb	%cl, rdram-0x80000000(%edi)
	jmp	.L15
	.size	write_rdramb_new, .-write_rdramb_new

.globl write_rdramh_new
	.type	write_rdramh_new, @function
write_rdramh_new:
	mov	address, %edi
	xor	$2, %edi
	movw	hword, %cx
	movw	%cx, rdram-0x80000000(%edi)
	jmp	.L15
	.size	write_rdramh_new, .-write_rdramh_new

.globl write_rdramd_new
	.type	write_rdramd_new, @function
write_rdramd_new:
	mov	address, %edi
	mov	dword+4, %ecx
	mov	dword, %edx
	mov	%ecx, rdram-0x80000000(%edi)
	mov	%edx, rdram-0x80000000+4(%edi)
	jmp	.L15
	.size	write_rdramd_new, .-write_rdramd_new

.globl do_invalidate
	.type	do_invalidate, @function
do_invalidate:
	mov	address, %edi
	mov	%edi, %ebx /* Return ebx to caller */
.L15:
	shr	$12, %edi
	cmpb	$1, invalid_code(%edi)
	je	.L16
	push	%edi
	call	invalidate_block
	pop	%edi
.L16:
	ret
	.size	do_invalidate, .-do_invalidate

.globl read_nomem_new
	.type	read_nomem_new, @function
read_nomem_new:
	mov	address, %edi
	mov	%edi, %ebx
	shr	$12, %edi
	mov	memory_map(,%edi,4),%edi
	mov	$0x8, %eax
	test	%edi, %edi
	js	tlb_exception
	mov	(%ebx,%edi,4), %ecx
	mov	%ecx, readmem_dword
	ret
	.size	read_nomem_new, .-read_nomem_new

.globl read_nomemb_new
	.type	read_nomemb_new, @function
read_nomemb_new:
	mov	address, %edi
	mov	%edi, %ebx
	shr	$12, %edi
	mov	memory_map(,%edi,4),%edi
	mov	$0x8, %eax
	test	%edi, %edi
	js	tlb_exception
	xor	$3, %ebx
	movzbl	(%ebx,%edi,4), %ecx
	mov	%ecx, readmem_dword
	ret
	.size	read_nomemb_new, .-read_nomemb_new

.globl read_nomemh_new
	.type	read_nomemh_new, @function
read_nomemh_new:
	mov	address, %edi
	mov	%edi, %ebx
	shr	$12, %edi
	mov	memory_map(,%edi,4),%edi
	mov	$0x8, %eax
	test	%edi, %edi
	js	tlb_exception
	xor	$2, %ebx
	movzwl	(%ebx,%edi,4), %ecx
	mov	%ecx, readmem_dword
	ret
	.size	read_nomemh_new, .-read_nomemh_new

.globl read_nomemd_new
	.type	read_nomemd_new, @function
read_nomemd_new:
	mov	address, %edi
	mov	%edi, %ebx
	shr	$12, %edi
	mov	memory_map(,%edi,4),%edi
	mov	$0x8, %eax
	test	%edi, %edi
	js	tlb_exception
	mov	4(%ebx,%edi,4), %ecx
	mov	(%ebx,%edi,4), %edx
	mov	%ecx, readmem_dword
	mov	%edx, readmem_dword+4
	ret
	.size	read_nomemd_new, .-read_nomemd_new

.globl write_nomem_new
	.type	write_nomem_new, @function
write_nomem_new:
	call	do_invalidate
	mov	memory_map(,%edi,4),%edi
	mov	word, %ecx
	mov	$0xc, %eax
	shl	$2, %edi
	jc	tlb_exception
	mov	%ecx, (%ebx,%edi)
	ret
	.size	write_nomem_new, .-write_nomem_new

.globl write_nomemb_new
	.type	write_nomemb_new, @function
write_nomemb_new:
	call	do_invalidate
	mov	memory_map(,%edi,4),%edi
	movb	byte, %cl
	mov	$0xc, %eax
	shl	$2, %edi
	jc	tlb_exception
	xor	$3, %ebx
	movb	%cl, (%ebx,%edi)
	ret
	.size	write_nomemb_new, .-write_nomemb_new

.globl write_nomemh_new
	.type	write_nomemh_new, @function
write_nomemh_new:
	call	do_invalidate
	mov	memory_map(,%edi,4),%edi
	movw	hword, %cx
	mov	$0xc, %eax
	shl	$2, %edi
	jc	tlb_exception
	xor	$2, %ebx
	movw	%cx, (%ebx,%edi)
	ret
	.size	write_nomemh_new, .-write_nomemh_new

.globl write_nomemd_new
	.type	write_nomemd_new, @function
write_nomemd_new:
	call	do_invalidate
	mov	memory_map(,%edi,4),%edi
	mov	dword+4, %edx
	mov	dword, %ecx
	mov	$0xc, %eax
	shl	$2, %edi
	jc	tlb_exception
	mov	%edx, (%ebx,%edi)
	mov	%ecx, 4(%ebx,%edi)
	ret
	.size	write_nomemd_new, .-write_nomemd_new

.globl tlb_exception
	.type	tlb_exception, @function
tlb_exception:
	/* eax = cause */
	/* ebx = address */
	/* ebp = instr addr + flags */
	mov	0x24(%esp), %ebp
/* Debug: 
	push	%ebp
	push	%ebx
	push	%eax
	call	tlb_debug
	pop	%eax
	pop	%ebx
	pop	%ebp
/* end debug */
	mov	reg_cop0+48, %esi
	mov	%ebp, %ecx
	mov	%ebp, %edx
	mov	%ebp, %edi
	shl	$31, %ebp
	or	$2, %esi
	shr	$12, %ecx
	or	%ebp, %eax
	and	$0xFFFFFFFC, %edx
	mov	memory_map(,%ecx,4), %ecx
	mov	%esi, reg_cop0+48 /* Status */
	mov	%eax, reg_cop0+52 /* Cause */
	mov	%edx, reg_cop0+56 /* EPC */
	mov	(%edx, %ecx, 4), %ecx
	add	$0x24, %esp
	mov	$0x6000022, %edx
	mov	%ecx, %ebp
	movswl	%cx, %eax
	shr	$26, %ecx
	shr	$21, %ebp
	sub	%eax, %ebx
	and	$0x1f, %ebp
	ror	%cl, %edx
	cmovc	reg(,%ebp,8), %ebx
	mov	%ebx, reg(,%ebp,8)
	lea	(%eax, %ebx), %ecx
	sar	$31, %ebx
	test	$2, %edi
	mov	%ecx, reg_cop0+32 /* BadVAddr */
	cmove	reg+4(,%ebp,8), %ebx
	push	$0x80000180
	mov	%ebx, reg+4(,%ebp,8)
	call	get_addr_ht
	pop	%esi
	movl	next_interupt, %edi
	movl	reg_cop0+36, %esi /* Count */
	movl	%edi, last_count
	subl	%edi, %esi
	jmp	*%eax
	.size	tlb_exception, .-tlb_exception
