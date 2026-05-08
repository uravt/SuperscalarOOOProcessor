.set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
# Test 1: Independent-chains / issue-width saturation.
# 8 fully-independent dependency chains on disjoint registers.
# Each chain has 8 serial RAW deps internally, but chains don't interact.
# Width-1 machine: IPC ~ 1. Width-N machine (N<=8): IPC ~ N if RS/ROB don't
# bottleneck. Sweep issue width to read ILP scaling directly.
    addi $31, $0, 20
loop:
    # ---- chain A ($1) ----
    addi $1, $1, 1
    addi $1, $1, 2
    addi $1, $1, 3
    addi $1, $1, 4
    # ---- chain B ($2) ----
    addi $2, $2, 1
    addi $2, $2, 2
    addi $2, $2, 3
    addi $2, $2, 4
    # ---- chain C ($3) ----
    addi $3, $3, 1
    addi $3, $3, 2
    addi $3, $3, 3
    addi $3, $3, 4
    # ---- chain D ($4) ----
    addi $4, $4, 1
    addi $4, $4, 2
    addi $4, $4, 3
    addi $4, $4, 4
    # ---- chain E ($5) ----
    addi $5, $5, 1
    addi $5, $5, 2
    addi $5, $5, 3
    addi $5, $5, 4
    # ---- chain F ($6) ----
    addi $6, $6, 1
    addi $6, $6, 2
    addi $6, $6, 3
    addi $6, $6, 4
    # ---- chain G ($7) ----
    addi $7, $7, 1
    addi $7, $7, 2
    addi $7, $7, 3
    addi $7, $7, 4
    # ---- chain H ($8) ----
    addi $8, $8, 1
    addi $8, $8, 2
    addi $8, $8, 3
    addi $8, $8, 4

    addi $31, $31, -1
    bne $31, $0, loop
	.end	__start
	.size	__start, .-__start
