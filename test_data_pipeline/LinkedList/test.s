.set noat

.text
.align 2
.globl __start
.ent __start

__start:

# -------------------------------------------------------------
# Matrix multiplication: C = A * B
# All matrices are 4x4 of 32-bit words (16 words = 64 bytes each)
#
# Memory layout:
#   A : base 0    (row-major, A[i][j] @ 0   + (i*4 + j)*4)
#   B : base 64   (row-major, B[i][j] @ 64  + (i*4 + j)*4)
#   C : base 128  (row-major, C[i][j] @ 128 + (i*4 + j)*4)
#
# After C is built, we compute auxiliary stats over C and store:
#   Stats region @ 256:
#     256: trace(C)         = C[0][0]+C[1][1]+C[2][2]+C[3][3]
#     260: sum of all C
#     264: max element of C
#     268: min element of C
#     272: sum of C's main diagonal
#     276: sum of C's anti-diagonal
#     280: frobenius-ish (sum of squares of C entries)
#     284: row-0 sum of C
#
# A =        B =
#  1  2  3  4    1  0  2  1
#  5  6  7  8    0  1  1  2
#  9 10 11 12    2  1  0  1
# 13 14 15 16    1  2  1  0
# -------------------------------------------------------------

# -------------------------------------------------------------
# Initialize A (16 words starting at address 0)
# -------------------------------------------------------------
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

# -------------------------------------------------------------
# Initialize B (16 words starting at address 64)
# -------------------------------------------------------------
addi $1, $0, 1
sw   $1, 64($0)
addi $1, $0, 0
sw   $1, 68($0)
addi $1, $0, 2
sw   $1, 72($0)
addi $1, $0, 1
sw   $1, 76($0)

addi $1, $0, 0
sw   $1, 80($0)
addi $1, $0, 1
sw   $1, 84($0)
addi $1, $0, 1
sw   $1, 88($0)
addi $1, $0, 2
sw   $1, 92($0)

addi $1, $0, 2
sw   $1, 96($0)
addi $1, $0, 1
sw   $1, 100($0)
addi $1, $0, 0
sw   $1, 104($0)
addi $1, $0, 1
sw   $1, 108($0)

addi $1, $0, 1
sw   $1, 112($0)
addi $1, $0, 2
sw   $1, 116($0)
addi $1, $0, 1
sw   $1, 120($0)
addi $1, $0, 0
sw   $1, 124($0)

# -------------------------------------------------------------
# Useful constants
# -------------------------------------------------------------
addi $20, $0, 4         # N (matrix dim)
addi $21, $0, 16        # row stride in bytes (N * 4)
addi $22, $0, 64        # base of B
addi $23, $0, 128       # base of C

# -------------------------------------------------------------
# Triple-nested loop:
#   for i in 0..3:
#     for j in 0..3:
#       acc = 0
#       for k in 0..3:
#         acc += A[i][k] * B[k][j]
#       C[i][j] = acc
#
# Register map:
#   $5  = i
#   $6  = j
#   $7  = k
#   $8  = acc
#   $9  = A row base   (i*16)
#   $10 = C row base   (i*16 + 128)
#   $11 = a_val = A[i][k]
#   $12 = b_val = B[k][j]
#   $13 = product (built via add-loop)
#   $14 = product counter
#   $15 = address scratch
#   $16 = address scratch
# -------------------------------------------------------------

addi $5, $0, 0          # i = 0
add  $9, $0, $0         # A row base
add  $10, $23, $0       # C row base = 128

i_loop:
addi $6, $0, 0          # j = 0

j_loop:
addi $8, $0, 0          # acc = 0
addi $7, $0, 0          # k = 0

k_loop:
# --- compute address of A[i][k] = $9 + k*4 ---
add  $15, $7, $7        # k*2
add  $15, $15, $15      # k*4
add  $15, $15, $9       # + A row base
lw   $11, 0($15)        # a_val

# --- compute address of B[k][j] = 64 + k*16 + j*4 ---
add  $16, $7, $7        # k*2
add  $16, $16, $16      # k*4
add  $16, $16, $16      # k*8
add  $16, $16, $16      # k*16
add  $16, $16, $22      # + base of B
add  $17, $6, $6        # j*2
add  $17, $17, $17      # j*4
add  $16, $16, $17      # + j*4
lw   $12, 0($16)        # b_val

# --- product = a_val * b_val via repeated addition ---
addi $13, $0, 0         # product = 0
add  $14, $12, $0       # counter = b_val
mul_loop:
beq  $14, $0, mul_done
add  $13, $13, $11
addi $14, $14, -1
beq  $0, $0, mul_loop
mul_done:

# --- acc += product ---
add  $8, $8, $13

# --- k++ ; if k < N, loop ---
addi $7, $7, 1
slt  $18, $7, $20
beq  $18, $0, k_done
beq  $0, $0, k_loop
k_done:

# --- store C[i][j] = acc, address = $10 + j*4 ---
add  $19, $6, $6        # j*2
add  $19, $19, $19      # j*4
add  $19, $19, $10      # + C row base
sw   $8, 0($19)

# --- j++ ; if j < N, loop ---
addi $6, $6, 1
slt  $18, $6, $20
beq  $18, $0, j_done
beq  $0, $0, j_loop
j_done:

# --- i++ ; advance row bases by 16; if i < N, loop ---
addi $5, $5, 1
add  $9,  $9,  $21      # A row base += 16
add  $10, $10, $21      # C row base += 16
slt  $18, $5, $20
beq  $18, $0, i_done
beq  $0, $0, i_loop
i_done:

# -------------------------------------------------------------
# Compute statistics over C
#   $24 = total sum
#   $25 = trace (main diag, same as diag here)
#   $26 = anti-diagonal sum
#   $27 = max
#   $28 = min
#   $29 = sum of squares
#   $30 = row-0 sum
# -------------------------------------------------------------
addi $24, $0, 0
addi $25, $0, 0
addi $26, $0, 0
addi $27, $0, 0          # max seed; all C entries should be >= 0 here
addi $28, $0, 9999       # min seed
addi $29, $0, 0
addi $30, $0, 0

addi $5, $0, 0           # i
stats_i:
addi $6, $0, 0           # j

stats_j:
# address of C[i][j] = 128 + i*16 + j*4
add  $15, $5, $5         # i*2
add  $15, $15, $15       # i*4
add  $15, $15, $15       # i*8
add  $15, $15, $15       # i*16
add  $15, $15, $23       # + base C
add  $16, $6, $6         # j*2
add  $16, $16, $16       # j*4
add  $15, $15, $16
lw   $11, 0($15)         # c_val

# total sum
add  $24, $24, $11

# row-0 sum
beq  $5, $0, in_row0
beq  $0, $0, after_row0
in_row0:
add  $30, $30, $11
after_row0:

# main diagonal: i == j
beq  $5, $6, on_diag
beq  $0, $0, after_diag
on_diag:
add  $25, $25, $11
after_diag:

# anti-diagonal: i + j == N-1, i.e., i + j == 3
add  $17, $5, $6
addi $18, $0, 3
beq  $17, $18, on_anti
beq  $0, $0, after_anti
on_anti:
add  $26, $26, $11
after_anti:

# max update
slt  $18, $27, $11
beq  $18, $0, max_skip
add  $27, $11, $0
max_skip:

# min update
slt  $18, $11, $28
beq  $18, $0, min_skip
add  $28, $11, $0
min_skip:

# square via repeated add (val * val)
addi $13, $0, 0          # sq accum
add  $14, $11, $0        # counter = val
sq_loop2:
beq  $14, $0, sq_done2
add  $13, $13, $11
addi $14, $14, -1
beq  $0, $0, sq_loop2
sq_done2:
add  $29, $29, $13

# j++
addi $6, $6, 1
slt  $18, $6, $20
beq  $18, $0, stats_j_done
beq  $0, $0, stats_j
stats_j_done:

# i++
addi $5, $5, 1
slt  $18, $5, $20
beq  $18, $0, stats_done
beq  $0, $0, stats_i
stats_done:

# -------------------------------------------------------------
# Store results at base 256
# -------------------------------------------------------------
sw $25, 256($0)          # trace
sw $24, 260($0)          # total sum
sw $27, 264($0)          # max
sw $28, 268($0)          # min
sw $25, 272($0)          # main diagonal sum (== trace)
sw $26, 276($0)          # anti-diagonal sum
sw $29, 280($0)          # sum of squares
sw $30, 284($0)          # row-0 sum

.end __start
.size __start, .-__start