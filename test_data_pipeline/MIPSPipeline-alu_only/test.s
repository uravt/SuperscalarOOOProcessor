  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   addi $1, $0, 7
   addi $2, $0, 3
   addi $3, $0, 12
   addi $4, $0, 9
   add $5, $1, $2
   add $6, $3, $4
   sub $7, $5, $6
   and $8, $1, $3
   or  $9, $2, $4
   xor $10, $5, $6
   sll $11, $1, 2
   srl $12, $3, 1
   add $13, $7, $8
   add $14, $9, $10
   add $15, $11, $12
   sub $16, $13, $14
   add $17, $15, $16
   addi $18, $17, 1
   addi $19, $18, 1
   addi $20, $19, 1
	.end	__start
	.size	__start, .-__start
