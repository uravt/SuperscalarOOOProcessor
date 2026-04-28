#ifndef CONFIG_H
#define CONFIG_H

namespace config {
    constexpr int NUM_PHYSICAL_REGISTERS = 96;
    constexpr int NUM_ARCHITECTURAL_REGISTERS = 32;
    constexpr int REORDER_BUFFER_SIZE = 64; 
    constexpr int PIPELINE_WIDTH = 8; 
    constexpr int INSTRUCTION_QUEUE_SIZE = 32;
    constexpr int NUM_ALUS = 8; 
    constexpr int NUM_MSHRS = 16;
    constexpr int LOAD_QUEUE_SIZE = 64;
    constexpr int STORE_QUEUE_SIZE = 64;

}

#endif