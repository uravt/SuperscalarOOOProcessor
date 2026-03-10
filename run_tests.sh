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
echo "==============================================================="
printf "%-25s | %-15s | %-15s\n" "Test Name" "Single (-O0)" "Pipelined (-O1)"
echo "---------------------------------------------------------------"

# Loop through all subdirectories inside test_data_pipeline
for dir in "$TEST_DIR"/*/; do
    
    # Grab just the folder name (e.g., "simple") to use as the test name
    test_name=$(basename "$dir")
    
    # Look for an assembly file (.s) in that directory
    asm_file=$(ls "$dir"*.s 2>/dev/null | head -n 1)
    
    # Skip directories that don't have a .s file
    if [[ ! -f "$asm_file" ]]; then
        continue
    fi
    
    # Set the compiled executable path/name (e.g., test_data_pipeline/simple/simple.out)
    exec_file="${dir}${test_name}.out"
    
    # 1. Compile the assembly file
    mipsel-linux-gnu-gcc -mips32 "$asm_file" -nostartfiles -Ttext=0 -o "$exec_file" 2>/dev/null
    
    # Check if compilation failed
    if [[ $? -ne 0 ]]; then
        printf "%-25s | %-15s | %-15s\n" "$test_name" "COMPILE ERROR" "COMPILE ERROR"
        continue
    fi
    
    # 2. Run Single Cycle (-O0)
    single_out=$($PROCESSOR --bmk="$exec_file" -O0 2>/dev/null)
    single_cycles=$(echo "$single_out" | grep -i "^CYCLE" | tail -n 1 | awk '{print $2}')
    
    # 3. Run Pipelined (-O1)
    pipe_out=$($PROCESSOR --bmk="$exec_file" -O1 2>/dev/null)
    pipe_cycles=$(echo "$pipe_out" | grep -i "^CYCLE" | tail -n 1 | awk '{print $2}')
    
    # Handle cases where the simulator crashes or outputs no cycles
    [[ -z "$single_cycles" ]] && single_cycles="ERR/CRASH"
    [[ -z "$pipe_cycles" ]] && pipe_cycles="ERR/CRASH"
    
    # 4. Print the formatted result row
    printf "%-25s | %-15s | %-15s\n" "$test_name" "$single_cycles" "$pipe_cycles"
done

echo "==============================================================="