.set noat

.text
.align 2
.globl __start
.ent __start

__start:

# =========================================================
# Matrix multiplication, ILP-friendly version
#
# C = A * B, both 4x4. Row-major, 4 bytes/element.
#   A : base 0     (A[i][j] @ i*16 + j*4)
#   B : base 64    (B[i][j] @ 64 + i*16 + j*4)
#   C : base 128
#   Stats : base 192
#
# Stats:
#   192  trace(C)
#   196  total sum of C
#   200  max of C
#   204  min of C
#   208  row-0 sum of C
#   212  anti-diagonal sum
#
# ILP design notes (vs. the prior SW-multiply version):
#   - Uses hardware `mul` instead of a serial add-loop.
#   - Inner k-loop fully unrolled: 4 independent multiplies
#     per output cell, then a 2-deep add tree. Short critical
#     path, lots of parallel ops the OOO core can co-issue.
#   - All 8 loads (4 from row of A, 4 from column of B) are
#     independent.
#   - Row of A is hoisted out of the j-loop and reused —
#     fewer redundant loads, less L1 traffic.
#   - i,j loops kept (predictable backward branches).
# =========================================================

# ---------- Initialize A ----------
addi $1, $0, 1
sw   $1, 0($0)
addi $1, $0, 2
sw   $1, 4($0)
addi $1, $0, 3
sw   $1, 8($0)
addi $1, $0, 4
sw   $1, 12($0)
addi $1, $0, 5
sw   $1, 16($0)
addi $1, $0, 6
sw   $1, 20($0)
addi $1, $0, 7
sw   $1, 24($0)
addi $1, $0, 8
sw   $1, 28($0)
addi $1, $0, 9
sw   $1, 32($0)
addi $1, $0, 10
sw   $1, 36($0)
addi $1, $0, 11
sw   $1, 40($0)
addi $1, $0, 12
sw   $1, 44($0)
addi $1, $0, 13
sw   $1, 48($0)
addi $1, $0, 14
sw   $1, 52($0)
addi $1, $0, 15
sw   $1, 56($0)
addi $1, $0, 16
sw   $1, 60($0)

# ---------- Initialize B ----------
addi $1, $0, 1
sw   $1, 64($0)
sw   $0, 68($0)
addi $1, $0, 2
sw   $1, 72($0)
addi $1, $0, 1
sw   $1, 76($0)

sw   $0, 80($0)
addi $1, $0, 1
sw   $1, 84($0)
sw   $1, 88($0)
addi $1, $0, 2
sw   $1, 92($0)

addi $1, $0, 2
sw   $1, 96($0)
addi $1, $0, 1
sw   $1, 100($0)
sw   $0, 104($0)
sw   $1, 108($0)

sw   $1, 112($0)
addi $1, $0, 2
sw   $1, 116($0)
addi $1, $0, 1
sw   $1, 120($0)
sw   $0, 124($0)

# ---------- Constants ----------
addi $20, $0, 4          # N
addi $22, $0, 64         # base of B
addi $23, $0, 128        # base of C

# =========================================================
# Compute C = A * B
#
# Reg map:
#   $5  i, $6 j
#   $7  A row base = i*16
#   $8  C row base = $7 + 128
#   $9  j*4
#   $10..$13   a0..a3   (row i of A, hoisted)
#   $14..$17   b0..b3   (column j of B)
#   $24..$27   p0..p3   (independent products)
#   $28,$29    partial sums
#   $30        store address
# =========================================================
addi $5, $0, 0

i_loop:
    sll  $7, $5, 4
    add  $8, $7, $23

    lw   $10, 0($7)
    lw   $11, 4($7)
    lw   $12, 8($7)
    lw   $13, 12($7)

    addi $6, $0, 0
j_loop:
    sll  $9, $6, 2
    add  $18, $22, $9        # &B[0][j]
    lw   $14, 0($18)
    lw   $15, 16($18)
    lw   $16, 32($18)
    lw   $17, 48($18)

    mul  $24, $10, $14
    mul  $25, $11, $15
    mul  $26, $12, $16
    mul  $27, $13, $17

    add  $28, $24, $25
    add  $29, $26, $27
    add  $28, $28, $29

    add  $30, $8, $9
    sw   $28, 0($30)

    addi $6, $6, 1
    slt  $19, $6, $20
    bne  $19, $0, j_loop

    addi $5, $5, 1
    slt  $19, $5, $20
    bne  $19, $0, i_loop

# =========================================================
# Stats over C
# =========================================================
addi $24, $0, 0          # total
addi $25, $0, 0          # trace
addi $26, $0, 0          # anti-diag
addi $27, $0, 0          # max seed (all C >= 0)
addi $28, $0, 9999       # min seed
addi $30, $0, 0          # row-0 sum

addi $5, $0, 0
stats_i:
    sll  $7, $5, 4
    add  $7, $7, $23

    addi $6, $0, 0
stats_j:
    sll  $9, $6, 2
    add  $15, $7, $9
    lw   $11, 0($15)

    add  $24, $24, $11

    bne  $5, $0, not_row0
    add  $30, $30, $11
not_row0:

    bne  $5, $6, not_diag
    add  $25, $25, $11
not_diag:

    add  $17, $5, $6
    addi $18, $0, 3
    bne  $17, $18, not_anti
    add  $26, $26, $11
not_anti:

    slt  $18, $27, $11
    beq  $18, $0, no_max
    add  $27, $11, $0
no_max:

    slt  $18, $11, $28
    beq  $18, $0, no_min
    add  $28, $11, $0
no_min:

    addi $6, $6, 1
    slt  $19, $6, $20
    bne  $19, $0, stats_j

    addi $5, $5, 1
    slt  $19, $5, $20
    bne  $19, $0, stats_i

addi $31, $0, 192
sw   $25, 0($31)         # trace
sw   $24, 4($31)         # total sum
sw   $27, 8($31)         # max
sw   $28, 12($31)        # min
sw   $30, 16($31)        # row-0 sum
sw   $26, 20($31)        # anti-diagonal sum

.end __start
.size __start, .-__start
