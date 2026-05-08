.set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
# Test 6: Predictable backward branch wrapping a high-ILP body.
# Outer bne is always-taken until the final iteration -> trivially predicted.
# Body is 16 mutually-independent ALU ops on disjoint registers (mix of
# addi/ori/andi/xor/sll/srl/add). Compared to ILP-issue_width this isolates
# loop-overhead cost; compared to a mispredict-heavy kernel it isolates the
# pure ILP ceiling of the body.

    addi $31, $0, 100
loop:
    addi $1, $1, 7
    ori  $2, $2, 0x0f
    andi $3, $3, 0x7f
    xor  $4, $4, $1
    sll  $5, $1, 2
    srl  $6, $2, 1
    add  $7, $1, $2
    addi $8, $8, 13

    addi $9,  $9,  3
    ori  $10, $10, 0x30
    andi $11, $11, 0x3f
    xor  $12, $12, $9
    sll  $13, $9, 3
    srl  $14, $10, 2
    add  $15, $9, $10
    addi $16, $16, 21

    addi $31, $31, -1
    bne $31, $0, loop
	.end	__start
	.size	__start, .-__start
