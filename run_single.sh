#!/bin/bash

PROCESSOR="./processor"
TEST_DIR="test_data_pipeline"

# Check if a test name was provided
if [[ -z "$1" ]]; then
    echo "Usage: ./run_single.sh <test_name>"
    echo "Example: ./run_single.sh simple"
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
mipsel-linux-gnu-gcc -mips32 "$asm_file" -nostartfiles -Ttext=0 -o "$exec_file"

if [[ $? -ne 0 ]]; then
    echo "Compilation failed!"
    exit 1
fi

# Run Single Cycle (-O0)
echo -e "\n======================================================="
echo "               SINGLE CYCLE OUTPUT (-O0)                 "
echo "======================================================="
single_out=$($PROCESSOR --bmk="$exec_file" -O0 2>&1)
echo "$single_out"

# Run Pipelined (-O1)
echo -e "\n======================================================="
echo "                PIPELINED OUTPUT (-O1)                   "
echo "======================================================="
pipe_out=$($PROCESSOR --bmk="$exec_file" -O1 2>&1)
echo "$pipe_out"

# Extract metrics
single_cycles=$(echo "$single_out" | grep -i "^CYCLE" | tail -n 1 | awk '{print $2}')
single_regs=$(echo "$single_out" | grep "^R\[" | tail -n 32)

pipe_cycles=$(echo "$pipe_out" | grep -i "^CYCLE" | tail -n 1 | awk '{print $2}')
pipe_regs=$(echo "$pipe_out" | grep "^R\[" | tail -n 32)

echo -e "\n======================================================="
echo "                    FINAL SUMMARY                        "
echo "======================================================="
printf "%-20s : %s\n" "Test Name" "$test_name"
printf "%-20s : %s\n" "Single-Cycle Cycles" "${single_cycles:-ERR/CRASH}"
printf "%-20s : %s\n" "Pipelined Cycles" "${pipe_cycles:-ERR/CRASH}"

# IPC / CPI for the pipelined run.
# -O0 is single-cycle (1 instr/cycle), so its cycle count == dynamic instr count.
if [[ "$single_cycles" =~ ^[0-9]+$ ]] && [[ "$pipe_cycles" =~ ^[0-9]+$ ]] && [[ "$pipe_cycles" -gt 0 ]]; then
    ipc=$(awk -v i="$single_cycles" -v c="$pipe_cycles" 'BEGIN{printf "%.4f", i/c}')
    cpi=$(awk -v i="$single_cycles" -v c="$pipe_cycles" 'BEGIN{printf "%.4f", c/i}')
    printf "%-20s : %s\n" "Instructions"   "$single_cycles"
    printf "%-20s : %s\n" "IPC (pipelined)" "$ipc"
    printf "%-20s : %s\n" "CPI (pipelined)" "$cpi"
else
    printf "%-20s : %s\n" "IPC (pipelined)" "N/A"
    printf "%-20s : %s\n" "CPI (pipelined)" "N/A"
fi

if [[ "$single_regs" == "$pipe_regs" ]] && [[ -n "$single_regs" ]]; then
    printf "%-20s : %s\n" "Reg Match" "YES"
else
    printf "%-20s : %s\n" "Reg Match" "NO"
fi
echo "======================================================="
