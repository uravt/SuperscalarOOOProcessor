.set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
# Test 4: Memory-level parallelism via independent loads.
# Pre-store 8 distinct words at disjoint addresses, then issue 8 independent
# lw's whose results are only consumed at the very end. With a non-blocking
# cache and an LSQ that doesn't serialize, the loads should overlap; IPC
# should be sensitive to MSHR count and LSQ size.

    # base pointers (disjoint cache lines: 64B apart)
    addi $20, $0, 1024
    addi $21, $20, 64
    addi $22, $20, 128
    addi $23, $20, 192
    addi $24, $20, 256
    addi $25, $20, 320
    addi $26, $20, 384
    addi $27, $20, 448

    # seed values
    addi $10, $0, 11
    addi $11, $0, 22
    addi $12, $0, 33
    addi $13, $0, 44
    addi $14, $0, 55
    addi $15, $0, 66
    addi $16, $0, 77
    addi $17, $0, 88

    sw $10, 0($20)
    sw $11, 0($21)
    sw $12, 0($22)
    sw $13, 0($23)
    sw $14, 0($24)
    sw $15, 0($25)
    sw $16, 0($26)
    sw $17, 0($27)

    addi $31, $0, 30
loop:
    # 8 independent loads — no addr or data dep between them
    lw $1, 0($20)
    lw $2, 0($21)
    lw $3, 0($22)
    lw $4, 0($23)
    lw $5, 0($24)
    lw $6, 0($25)
    lw $7, 0($26)
    lw $8, 0($27)

    # consume only at the end (balanced add tree: short critical path)
    add $1, $1, $2
    add $3, $3, $4
    add $5, $5, $6
    add $7, $7, $8
    add $1, $1, $3
    add $5, $5, $7
    add $1, $1, $5

    addi $31, $31, -1
    bne $31, $0, loop
	.end	__start
	.size	__start, .-__start
