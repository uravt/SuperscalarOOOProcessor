#include "memory.h"
#include "regfile.h"
#include "ALU.h"
#include "control.h"
class Processor {
    private:
        int opt_level;
        ALU alu;
        control_t control;
        Memory *memory;
        Registers regfile;
        // add other structures as needed
        struct DecodeData {
            int opcode;
            int rs;
            int rd;
            int shamt;
            int funct;
            uint32_t imm;
            int addr;
            uint32_t read_data_1;
            uint32_t read_data_2;
        };

        // pipelined processor
        void* fetch_stage(void* arg);
        void* decode_stage(void* arg);
        void* execute_stage(void* arg);
        void* memory_stage(void* arg);
        void* writeback_stage(void* arg);
        // add private functions
        void single_cycle_processor_advance();
        void pipelined_processor_advance();
 
    public:
        Processor(Memory *mem) { regfile.pc = 0; memory = mem;}

        // Get PC
        uint32_t getPC() { return regfile.pc; }

        // Prints the Register File
        void printRegFile() { regfile.print(); }
        
        // Initializes the processor appropriately based on the optimization level
        void initialize(int opt_level);

        // Advances the processor to an appropriate state every cycle
        void advance(); 
};
