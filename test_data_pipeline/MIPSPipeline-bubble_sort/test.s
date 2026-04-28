  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   # Bubble sort 6 ints in ascending order.
   # Array at base 0: [5, 2, 8, 1, 6, 3]  -> [1, 2, 3, 5, 6, 8]

   addi $1, $0, 5
   sw   $1, 0($0)
   addi $1, $0, 2
   sw   $1, 4($0)
   addi $1, $0, 8
   sw   $1, 8($0)
   addi $1, $0, 1
   sw   $1, 12($0)
   addi $1, $0, 6
   sw   $1, 16($0)
   addi $1, $0, 3
   sw   $1, 20($0)

   addi $5, $0, 6        # n
   addi $6, $0, 0        # i = 0
outer:
   addi $7, $0, 0        # j = 0
   sub  $8, $5, $6
   addi $8, $8, -1       # bound = n - i - 1
inner:
   beq  $7, $8, inner_end
   sll  $9, $7, 2        # byte offset = j*4
   lw   $10, 0($9)       # a[j]
   addi $11, $9, 4
   lw   $12, 0($11)      # a[j+1]
   slt  $13, $12, $10    # a[j+1] < a[j] ?
   beq  $13, $0, no_swap
   sw   $12, 0($9)
   sw   $10, 0($11)
no_swap:
   addi $7, $7, 1
   j    inner
inner_end:
   addi $6, $6, 1
   bne  $6, $5, outer
done:
	.end	__start
	.size	__start, .-__start
