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

# Print table header
echo "=========================================================================="
printf "%-25s | %-12s | %-15s | %-10s\n" "Test Name" "Single (-O0)" "Pipelined (-O1)" "Reg Match?"
echo "--------------------------------------------------------------------------"

# Loop through all subdirectories inside test_data_pipeline
for dir in "$TEST_DIR"/*/; do
    
    # Grab just the folder name
    test_name=$(basename "$dir")
    
    # Look for an assembly file (.s)
    asm_file=$(ls "$dir"*.s 2>/dev/null | head -n 1)
    if [[ ! -f "$asm_file" ]]; then
        continue
    fi
    
    exec_file="${dir}${test_name}.out"
    
    # 1. Compile the assembly file
    mipsel-linux-gnu-gcc -mips32 "$asm_file" -nostartfiles -Ttext=0 -o "$exec_file" 2>/dev/null
    
    if [[ $? -ne 0 ]]; then
        printf "%-25s | %-12s | %-15s | %-10s\n" "$test_name" "COMPILE ERR" "COMPILE ERR" "N/A"
        continue
    fi
    
    # 2. Run Single Cycle (-O0)
    single_out=$($PROCESSOR --bmk="$exec_file" -O0 2>/dev/null)
    single_cycles=$(echo "$single_out" | grep -i "^CYCLE" | tail -n 1 | awk '{print $2}')
    # Extract the final 32 register values
    single_regs=$(echo "$single_out" | grep "^R\[" | tail -n 32)
    
    # 3. Run Pipelined (-O1)
    pipe_out=$($PROCESSOR --bmk="$exec_file" -O1 2>/dev/null)
    pipe_cycles=$(echo "$pipe_out" | grep -i "^CYCLE" | tail -n 1 | awk '{print $2}')
    # Extract the final 32 register values
    pipe_regs=$(echo "$pipe_out" | grep "^R\[" | tail -n 32)
    
    # Handle crashes/empty outputs
    [[ -z "$single_cycles" ]] && single_cycles="ERR/CRASH"
    [[ -z "$pipe_cycles" ]] && pipe_cycles="ERR/CRASH"
    
    # 4. Compare the final register states
    if [[ "$single_regs" == "$pipe_regs" ]] && [[ -n "$single_regs" ]]; then
        match_status="YES"
    else
        match_status="NO"
    fi
    
    # 5. Print the formatted result row
    printf "%-25s | %-12s | %-15s | %-10s\n" "$test_name" "$single_cycles" "$pipe_cycles" "$match_status"
    
    # 6. Cleanup: delete the compiled executable so folders stay clean
    rm -f "$exec_file"
done

echo "=========================================================================="