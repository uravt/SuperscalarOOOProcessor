.set noat

.text
.align 2
.globl __start
.ent __start

__start:

# -------------------------------------------------------------
# Intensive bubble sort test
#
# Sorts a 16-element array of 32-bit words in ascending order.
# Then performs a verification pass and computes statistics.
#
# Memory layout:
#   Array A    : base 0    (16 words = 64 bytes)
#   Stats      : base 128
#     128: number of swaps performed
#     132: number of comparisons performed
#     136: number of outer passes that did at least one swap
#     140: sum of array (invariant: same before and after sort)
#     144: min element (= A[0] after sort)
#     148: max element (= A[15] after sort)
#     152: is_sorted flag (1 if verified sorted, 0 otherwise)
#     156: number of inversions before sort (== final swap count
#          for adjacent-swap bubble sort, useful sanity check)
#
# Initial array (deliberately mixed: not sorted, not reversed,
# duplicates present, gives lots of swap activity):
#   [42, 7, 19, 99, 3, 56, 12, 88, 25, 71, 4, 33, 60, 15, 50, 28]
#
# Why this stresses an OOO core:
#   - The compare-and-swap branch is purely data-dependent. A
#     bimodal predictor will struggle; a two-level local
#     predictor with enough history might pick up patterns but
#     this input is mostly random. Expect frequent mispredicts
#     and ROB squashes.
#   - Loop-back branches at two nesting levels with different
#     trip counts each iteration (inner trip count decreases as
#     outer index advances).
#   - Heavy load/store traffic with potential aliasing: every
#     swap reads A[j] and A[j+1] and stores them back, then the
#     next iteration loads A[j+1] (now updated) again. Tests
#     store-to-load forwarding in the LSQ.
#   - "swapped" flag creates a loop-carried scalar dep that
#     limits how aggressively the outer loop can speculate
#     past the inner loop's exit.
# -------------------------------------------------------------

# -------------------------------------------------------------
# Initialize array
# -------------------------------------------------------------
addi $1, $0, 42
sw   $1, 0($0)
addi $1, $0, 7
sw   $1, 4($0)
addi $1, $0, 19
sw   $1, 8($0)
addi $1, $0, 99
sw   $1, 12($0)
addi $1, $0, 3
sw   $1, 16($0)
addi $1, $0, 56
sw   $1, 20($0)
addi $1, $0, 12
sw   $1, 24($0)
addi $1, $0, 88
sw   $1, 28($0)
addi $1, $0, 25
sw   $1, 32($0)
addi $1, $0, 71
sw   $1, 36($0)
addi $1, $0, 4
sw   $1, 40($0)
addi $1, $0, 33
sw   $1, 44($0)
addi $1, $0, 60
sw   $1, 48($0)
addi $1, $0, 15
sw   $1, 52($0)
addi $1, $0, 50
sw   $1, 56($0)
addi $1, $0, 28
sw   $1, 60($0)

# -------------------------------------------------------------
# Pre-sort pass: count inversions and compute initial sum.
# This pass is independent of the sort (read-only), and is
# placed before the sort so its loads can't alias with the
# sort's stores. Gives the OOO core a clean warmup.
#
# Inversions: count of pairs (i,j) with i<j and A[i] > A[j].
# This must equal the total number of swaps a correct
# adjacent-swap bubble sort performs, so it doubles as a
# self-check.
#
# Registers:
#   $2  = inversion count
#   $3  = initial sum
#   $4  = i (outer)
#   $5  = j (inner)
#   $6  = N = 16
#   $7  = end byte offset = 64
#   $8  = address of A[i]
#   $9  = A[i]
#   $10 = address of A[j]
#   $11 = A[j]
#   $12 = scratch (slt result)
# -------------------------------------------------------------
addi $2, $0, 0           # inversions = 0
addi $3, $0, 0           # sum = 0
addi $4, $0, 0           # i = 0
addi $6, $0, 16          # N
addi $7, $0, 64          # end byte offset

addi $8, $0, 0           # &A[0]

inv_outer:
lw   $9, 0($8)           # A[i]
add  $3, $3, $9          # sum += A[i]   (independent of inversion work)

# inner: j = i+1 .. N-1
addi $10, $8, 4          # &A[i+1]

inv_inner:
beq  $10, $7, inv_inner_done
lw   $11, 0($10)
slt  $12, $11, $9        # A[j] < A[i] ?
beq  $12, $0, no_inv
addi $2, $2, 1           # count inversion
no_inv:
addi $10, $10, 4
beq  $0, $0, inv_inner
inv_inner_done:

addi $4, $4, 1
addi $8, $8, 4
slt  $12, $4, $6
beq  $12, $0, inv_done
beq  $0, $0, inv_outer
inv_done:

# Save inversion count and initial sum; we'll re-check after sort.
sw $2, 156($0)           # inversion count

# -------------------------------------------------------------
# Bubble sort proper.
#
# Outer loop: keep going while a pass produced any swap.
# Inner loop: walk j from 0 to (N-1-pass) - 1, swap adjacent
# pairs that are out of order.
#
# Optimization: shrink the inner-loop endpoint each outer pass,
# since after pass p the last p elements are guaranteed sorted.
#
# Registers:
#   $13 = swap count
#   $14 = comparison count
#   $15 = passes-with-swap count
#   $16 = swapped flag (current pass)
#   $17 = inner end byte offset (decreases by 4 each outer pass)
#   $18 = j byte address
#   $19 = A[j]
#   $20 = A[j+1]
#   $21 = scratch (slt result)
# -------------------------------------------------------------
addi $13, $0, 0          # swaps
addi $14, $0, 0          # compares
addi $15, $0, 0          # passes with swap
addi $17, $0, 60         # inner end = (N-1)*4 = 60 initially

outer:
addi $16, $0, 0          # swapped = 0
addi $18, $0, 0          # j = 0 (byte addr of A[0])

inner:
beq  $18, $17, inner_done
lw   $19, 0($18)         # A[j]
lw   $20, 4($18)         # A[j+1]
addi $14, $14, 1         # compares++

slt  $21, $20, $19       # A[j+1] < A[j] ?
beq  $21, $0, no_swap
# swap: store A[j+1] at j, A[j] at j+1
sw   $20, 0($18)
sw   $19, 4($18)
addi $13, $13, 1         # swaps++
addi $16, $0, 1          # swapped = 1
no_swap:

addi $18, $18, 4
beq  $0, $0, inner
inner_done:

# If at least one swap happened this pass, count it and continue.
beq  $16, $0, sort_done
addi $15, $15, 1
addi $17, $17, -4        # shrink inner range
beq  $0, $0, outer

sort_done:

# -------------------------------------------------------------
# Verification pass:
#   - Walk the array, confirm A[i] <= A[i+1] for all i.
#   - Recompute sum (must equal pre-sort sum: invariant).
#   - Capture min (A[0]) and max (A[N-1]).
#
# Registers:
#   $22 = is_sorted flag (start at 1, clear on violation)
#   $23 = post-sort sum
#   $24 = j addr
#   $25 = A[j]
#   $26 = A[j+1]
#   $27 = scratch
# -------------------------------------------------------------
addi $22, $0, 1          # is_sorted = 1
addi $23, $0, 0          # post sum
addi $24, $0, 0          # j addr

verify_loop:
beq  $24, $7, verify_sum_done    # sum loop runs full length
lw   $25, 0($24)
add  $23, $23, $25
addi $24, $24, 4
beq  $0, $0, verify_loop
verify_sum_done:

# order check: j = 0 .. N-2
addi $24, $0, 0
addi $28, $0, 60         # stop when j == 60 (last valid pair is at 56)

order_loop:
beq  $24, $28, order_done
lw   $25, 0($24)
lw   $26, 4($24)
slt  $27, $26, $25       # if A[j+1] < A[j], not sorted
beq  $27, $0, order_ok
addi $22, $0, 0          # clear flag
order_ok:
addi $24, $24, 4
beq  $0, $0, order_loop
order_done:

# min = A[0], max = A[15]
lw $29, 0($0)
lw $30, 60($0)

# -------------------------------------------------------------
# Store stats
# -------------------------------------------------------------
sw $13, 128($0)          # swaps
sw $14, 132($0)          # compares
sw $15, 136($0)          # passes with swap
sw $23, 140($0)          # post-sort sum
sw $29, 144($0)          # min
sw $30, 148($0)          # max
sw $22, 152($0)          # is_sorted
# 156 already holds inversion count

.end __start
.size __start, .-__start