.set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
# Test 5: Parallel store->load forwarding.
# Four independent (sw, lw) pairs to disjoint addresses, interleaved. Each
# lw matches a prior sw at the same address (forward), but the four pairs
# share no address or data deps. A serialized STLF path will collapse to
# IPC ~ 1; a parallel LSQ should overlap them.

    addi $20, $0, 2048
    addi $21, $20, 64
    addi $22, $20, 128
    addi $23, $20, 192

    addi $31, $0, 30
loop:
    addi $10, $10, 1
    addi $11, $11, 2
    addi $12, $12, 3
    addi $13, $13, 4

    # interleaved independent sw/lw pairs
    sw $10, 0($20)
    sw $11, 0($21)
    sw $12, 0($22)
    sw $13, 0($23)

    lw $1, 0($20)
    lw $2, 0($21)
    lw $3, 0($22)
    lw $4, 0($23)

    # second wave to keep LSQ pressure up
    addi $10, $10, 5
    addi $11, $11, 6
    addi $12, $12, 7
    addi $13, $13, 8

    sw $10, 0($20)
    sw $11, 0($21)
    sw $12, 0($22)
    sw $13, 0($23)

    lw $5, 0($20)
    lw $6, 0($21)
    lw $7, 0($22)
    lw $8, 0($23)

    add $1, $1, $5
    add $2, $2, $6
    add $3, $3, $7
    add $4, $4, $8

    addi $31, $31, -1
    bne $31, $0, loop
	.end	__start
	.size	__start, .-__start
