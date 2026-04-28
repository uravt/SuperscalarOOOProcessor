  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   # Singly linked list of 5 nodes, each {value, next}.
   # Nodes start at addr 4 so addr 0 can serve as the null sentinel.
   # Layout (addr: contents):
   #   4: val=10   8: next=12
   #  12: val=20  16: next=20
   #  20: val=30  24: next=28
   #  28: val=40  32: next=36
   #  36: val=50  40: next=0  (null)
   # Traverse and accumulate sum into $3.

   addi $1, $0, 10
   sw   $1, 4($0)
   addi $1, $0, 12
   sw   $1, 8($0)

   addi $1, $0, 20
   sw   $1, 12($0)
   addi $1, $0, 20
   sw   $1, 16($0)

   addi $1, $0, 30
   sw   $1, 20($0)
   addi $1, $0, 28
   sw   $1, 24($0)

   addi $1, $0, 40
   sw   $1, 28($0)
   addi $1, $0, 36
   sw   $1, 32($0)

   addi $1, $0, 50
   sw   $1, 36($0)
   addi $1, $0, 0
   sw   $1, 40($0)

   addi $2, $0, 4       # head pointer
   addi $3, $0, 0       # running sum
traverse:
   beq  $2, $0, done
   lw   $4, 0($2)       # node value
   add  $3, $3, $4
   lw   $2, 4($2)       # next pointer
   j    traverse
done:
	.end	__start
	.size	__start, .-__start
