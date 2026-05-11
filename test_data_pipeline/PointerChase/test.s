# Pointer-chase benchmark.
#
# Builds a 32-node circular linked list, then walks it for many iterations.
# Each load's address depends on the previous load's result, so memory
# accesses are serialized through a true data dependency. The OOO engine
# cannot overlap them with MSHRs — wider issue width gains nothing on the
# critical path. Speedup vs width=1 should stay near 1x.
#
# Layout: 32 nodes at base 0x4000, stride 0x400 (1 KB).
# Working set = 32 KB (right at L1 capacity), so every chase step pays
# at least L1 latency, and many spill into L2.

.set noat
    .text
    .align  2
    .globl  __start
    .ent    __start
    .type   __start, @function
__start:
    # ---------------- setup ----------------
    # $1  = head address  (0x4000)
    # $4  = current write pointer
    # $5  = node counter
    # $6  = next-pointer value
    addi $1, $0, 0x10
    sll  $1, $1, 10        # $1 = 0x4000

    addi $4, $1, 0
    addi $5, $0, 31        # write 31 forward links; close cycle below
    addi $6, $1, 0x400

setup_loop:
    sw   $6, 0($4)
    addi $4, $4, 0x400
    addi $6, $6, 0x400
    addi $5, $5, -1
    bne  $5, $0, setup_loop

    # close the cycle: last node points back to head
    sw   $1, 0($4)         # $4 == 0x4000 + 31*0x400 == 0x11C00 (last node)

    # ---------------- chase ----------------
    # Outer iterations * 32 nodes per pass = total dependent loads.
    addi $2, $1, 0         # current pointer = head
    addi $7, $0, 100       # 100 passes -> 3200 dependent loads

chase_outer:
    addi $8, $0, 32
chase_inner:
    lw   $2, 0($2)         # SERIAL DEPENDENCY: next addr <- prev result
    addi $8, $8, -1
    bne  $8, $0, chase_inner

    addi $7, $7, -1
    bne  $7, $0, chase_outer

    .end    __start
    .size   __start, .-__start
