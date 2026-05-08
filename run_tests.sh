#!/bin/bash

# Ensure the processor simulator exists and is executable
PROCESSOR="./processor"
TEST_DIR="test_data_pipeline"

if [[ ! -x "$PROCESSOR" ]]; then
    echo "Error: Cannot find or execute $PROCESSOR in the current directory."
    exit 1
fi

if [[ ! -d "$TEST_DIR" ]]; then
    echo "Error: Directory '$TEST_DIR' does not exist."
    exit 1
fi

# Note: -O0 is single-cycle (1 instruction per cycle), so we use its cycle
# count as the dynamic instruction count for IPC/CPI on the pipelined run.

echo "================================================================================================"
printf "%-25s | %-12s | %-15s | %-10s | %-6s | %-6s\n" \
    "Test Name" "Single (-O0)" "Pipelined (-O1)" "Reg Match?" "IPC" "CPI"
echo "------------------------------------------------------------------------------------------------"

for dir in "$TEST_DIR"/*/; do
    test_name=$(basename "$dir")

    asm_file=$(ls "$dir"*.s 2>/dev/null | head -n 1)
    if [[ ! -f "$asm_file" ]]; then
        continue
    fi

    exec_file="${dir}${test_name}.out"

    mipsel-linux-gnu-gcc -mips32 "$asm_file" -nostartfiles -Ttext=0 -o "$exec_file" 2>/dev/null

    if [[ $? -ne 0 ]]; then
        printf "%-25s | %-12s | %-15s | %-10s | %-6s | %-6s\n" \
            "$test_name" "COMPILE ERR" "COMPILE ERR" "N/A" "N/A" "N/A"
        continue
    fi

    single_out=$($PROCESSOR --bmk="$exec_file" -O0 2>/dev/null)
    single_cycles=$(echo "$single_out" | grep -i "^CYCLE" | tail -n 1 | awk '{print $2}')
    single_regs=$(echo "$single_out" | grep "^R\[" | tail -n 32)

    pipe_out=$($PROCESSOR --bmk="$exec_file" -O1 2>/dev/null)
    pipe_cycles=$(echo "$pipe_out" | grep -i "^CYCLE" | tail -n 1 | awk '{print $2}')
    pipe_regs=$(echo "$pipe_out" | grep "^R\[" | tail -n 32)

    [[ -z "$single_cycles" ]] && single_cycles="ERR/CRASH"
    [[ -z "$pipe_cycles" ]] && pipe_cycles="ERR/CRASH"

    if [[ "$single_regs" == "$pipe_regs" ]] && [[ -n "$single_regs" ]]; then
        match_status="YES"
    else
        match_status="NO"
    fi

    # IPC/CPI for the pipelined run, using single-cycle count as instr count
    if [[ "$single_cycles" =~ ^[0-9]+$ ]] && [[ "$pipe_cycles" =~ ^[0-9]+$ ]] && [[ "$pipe_cycles" -gt 0 ]]; then
        ipc=$(awk -v i="$single_cycles" -v c="$pipe_cycles" 'BEGIN{printf "%.3f", i/c}')
        cpi=$(awk -v i="$single_cycles" -v c="$pipe_cycles" 'BEGIN{printf "%.3f", c/i}')
    else
        ipc="N/A"
        cpi="N/A"
    fi

    printf "%-25s | %-12s | %-15s | %-10s | %-6s | %-6s\n" \
        "$test_name" "$single_cycles" "$pipe_cycles" "$match_status" "$ipc" "$cpi"

    rm -f "$exec_file"
done

echo "================================================================================================"
