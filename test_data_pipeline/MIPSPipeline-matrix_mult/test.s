  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   # 3x3 * 3x3 matrix multiply, integer only, no mul/div.
   # A at base 0 (9 words), B at base 36, C at base 72.
   # Inner multiply via shift-and-add.

   # ---- init A ----
   addi $1, $0, 1
   sw $1, 0($0)
   addi $1, $0, 2
   sw $1, 4($0)
   addi $1, $0, 3
   sw $1, 8($0)
   addi $1, $0, 4
   sw $1, 12($0)
   addi $1, $0, 5
   sw $1, 16($0)
   addi $1, $0, 6
   sw $1, 20($0)
   addi $1, $0, 1
   sw $1, 24($0)
   addi $1, $0, 0
   sw $1, 28($0)
   addi $1, $0, 2
   sw $1, 32($0)

   # ---- init B ----
   addi $1, $0, 2
   sw $1, 36($0)
   addi $1, $0, 0
   sw $1, 40($0)
   addi $1, $0, 1
   sw $1, 44($0)
   addi $1, $0, 1
   sw $1, 48($0)
   addi $1, $0, 3
   sw $1, 52($0)
   addi $1, $0, 0
   sw $1, 56($0)
   addi $1, $0, 0
   sw $1, 60($0)
   addi $1, $0, 1
   sw $1, 64($0)
   addi $1, $0, 2
   sw $1, 68($0)

   # i_off in $5: 0, 12, 24
   addi $5, $0, 0
i_loop:
   # j_off in $6: 0, 4, 8
   addi $6, $0, 0
j_loop:
   addi $7, $0, 0       # sum
   addi $8, $0, 0       # k_off (0,4,8)
   addi $9, $0, 0       # b_row_off (0,12,24)
k_loop:
   # a = A[i_off + k_off]
   add  $10, $5, $8
   lw   $11, 0($10)
   # b = B[36 + b_row_off + j_off]
   addi $12, $0, 36
   add  $12, $12, $9
   add  $12, $12, $6
   lw   $13, 0($12)

   # ---- multiply $11 * $13, accumulate into $7 ----
   add  $14, $0, $11    # mcand
   add  $15, $0, $13    # mplier
mul_lp:
   beq  $15, $0, mul_done
   andi $16, $15, 1
   beq  $16, $0, mul_skip
   add  $7, $7, $14
mul_skip:
   sll  $14, $14, 1
   srl  $15, $15, 1
   j    mul_lp
mul_done:

   addi $8, $8, 4
   addi $9, $9, 12
   addi $17, $0, 12
   bne  $8, $17, k_loop

   # C[i_off + j_off] = sum
   addi $18, $0, 72
   add  $18, $18, $5
   add  $18, $18, $6
   sw   $7, 0($18)

   addi $6, $6, 4
   addi $19, $0, 12
   bne  $6, $19, j_loop

   addi $5, $5, 12
   addi $20, $0, 36
   bne  $5, $20, i_loop
end:
	.end	__start
	.size	__start, .-__start
