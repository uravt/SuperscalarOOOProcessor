#ifndef CONFIG_H
#define CONFIG_H

namespace config {
    constexpr int NUM_PHYSICAL_REGISTERS = 96;
    constexpr int NUM_ARCHITECTURAL_REGISTERS = 32;
    constexpr int REORDER_BUFFER_SIZE = 32; // from the MIPS R10000
    constexpr int PIPELINE_WIDTH = 2; 
    constexpr int INSTRUCTION_QUEUE_SIZE = 16;
    constexpr int NUM_ALUS = 2; 
    constexpr int NUM_MSHRS = 16; 
}

#endif