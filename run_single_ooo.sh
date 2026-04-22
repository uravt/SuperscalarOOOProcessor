#!/bin/bash

PROCESSOR="./processor"
TEST_DIR="test_data_pipeline"

# Check if a test name was provided
if [[ -z "$1" ]]; then
    echo "Usage: ./run_single_ooo.sh <test_name>"
    echo "Example: ./run_single_ooo.sh MIPSPipeline-alu_only"
    exit 1
fi

test_name="$1"
target_dir="${TEST_DIR}/${test_name}"

if [[ ! -x "$PROCESSOR" ]]; then
    echo "Error: Cannot find or execute $PROCESSOR."
    exit 1
fi

if [[ ! -d "$target_dir" ]]; then
    echo "Error: Test directory '$target_dir' does not exist."
    exit 1
fi

# Find the assembly file
asm_file=$(ls "$target_dir"/*.s 2>/dev/null | head -n 1)
if [[ ! -f "$asm_file" ]]; then
    echo "Error: No .s assembly file found in $target_dir"
    exit 1
fi

exec_file="${target_dir}/${test_name}.out"

echo "Compiling $asm_file..."
compile_out=$(mipsel-linux-gnu-gcc -mips32 "$asm_file" -nostartfiles -Ttext=0 -o "$exec_file" 2>&1)
compile_rc=$?

if [[ $compile_rc -ne 0 ]]; then
    echo "Compilation failed (exit $compile_rc):"
    echo "$compile_out"
    exit 1
fi

# Run Single Cycle (-O0)
echo -e "\n======================================================="
echo "               SINGLE CYCLE OUTPUT (-O0)                 "
echo "======================================================="
single_out=$($PROCESSOR --bmk="$exec_file" -O0 2>&1)
single_rc=$?
echo "$single_out"
if [[ $single_rc -ne 0 ]]; then
    echo ">>> Single-cycle run exited with code $single_rc"
fi

# Run Out-of-Order (-O2)
echo -e "\n======================================================="
echo "               OUT-OF-ORDER OUTPUT (-O2)                 "
echo "======================================================="
ooo_out=$($PROCESSOR --bmk="$exec_file" -O2 2>&1)
ooo_rc=$?
echo "$ooo_out"
if [[ $ooo_rc -ne 0 ]]; then
    echo ">>> OOO run exited with code $ooo_rc"
fi

# Extract metrics
single_cycles=$(echo "$single_out" | grep -i "^CYCLE" | tail -n 1 | awk '{print $2}')
single_regs=$(echo "$single_out" | grep "^R\[" | tail -n 32)

ooo_cycles=$(echo "$ooo_out" | grep -i "^CYCLE" | tail -n 1 | awk '{print $2}')
ooo_regs=$(echo "$ooo_out" | grep "^R\[" | tail -n 32)

if [[ "$single_regs" == "$ooo_regs" ]] && [[ -n "$single_regs" ]]; then
    match_status="YES"
else
    match_status="NO"
fi

echo -e "\n======================================================="
echo "                    FINAL SUMMARY                        "
echo "======================================================="
printf "%-20s : %s\n" "Test Name" "$test_name"
printf "%-20s : %s\n" "Single-Cycle Cycles" "${single_cycles:-ERR/CRASH}"
printf "%-20s : %s\n" "OOO Cycles" "${ooo_cycles:-ERR/CRASH}"
printf "%-20s : %s\n" "Reg Match?" "$match_status"

rm -f "$exec_file"
