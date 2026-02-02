# Variables
GCC      = mipsel-linux-gnu-gcc
# Added -g for debug symbols and -O0 to prevent instruction reordering
GCC_OPTS = -mips32 -nostartfiles -Ttext=0 -g -O0
OBJDUMP  = mipsel-linux-gnu-objdump
# Added -d (disassemble), -S (source interleave), and -l (line numbers)
DUMP_OPTS = -d -S -l

BIN_DIR  = ./bin
DUMP_DIR = ./dump

# 1. Find all test.s files in subdirectories
SRCS = $(wildcard */test.s)

# 2. Derive binary names: MIPSPipeline-branch/test.s -> ./bin/MIPSPipeline-branch
BINS  = $(patsubst %/test.s, $(BIN_DIR)/%.mips, $(SRCS))

# 3. Derive dump names: ./bin/MIPSPipeline-branch -> ./dump/MIPSPipeline-branch.txt
DUMPS = $(patsubst $(BIN_DIR)/%, $(DUMP_DIR)/%.dump, $(BINS))

.PHONY: all clean dirs

# Default target
all: dirs $(BINS) $(DUMPS)

# Create directories
dirs:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(DUMP_DIR)

# Rule to compile: takes the dir/test.s and outputs to bin/dir
$(BIN_DIR)/%.mips: %/test.s
	$(GCC) $(GCC_OPTS) $< -o $@

# Rule to generate dump: takes the binary and creates a text dump
$(DUMP_DIR)/%.dump: $(BIN_DIR)/%
	$(OBJDUMP) $(DUMP_OPTS) $< > $@

clean:
	@echo "Cleaning ./bin and ./dump"
	rm -rf $(BIN_DIR) $(DUMP_DIR)
