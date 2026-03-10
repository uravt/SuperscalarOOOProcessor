#include "memory.h"
#include "regfile.h"
#include "ALU.h"
#include "control.h"
#include <sstream>
#include <queue>
#include <set>
class Processor {
    private:
        int opt_level;
        ALU alu;
        control_t control;
        Memory *memory;
        Registers regfile;
        // add other structures as needed
        //pipeline registers
        //values got based on diagram registers in and out
        //data types based on single processor implementation

    struct IF_ID
    {
        uint32_t pc;
        uint32_t instruction;

        std::string toString() const
        {
            std::ostringstream oss;
            oss << "pc:" << pc
                << "|instruction:" << instruction;
            return oss.str();
        }
    };

    struct ID_EX
    {
        uint32_t pc;
        uint32_t read_data_1;
        uint32_t read_data_2;
        uint32_t imm;     // instruction[15-0]
        int rt;           // instruction[20-16]
        int rd;           // instruction[15-11]
        uint32_t opcode;
        uint32_t funct;
        uint32_t shamt;
        uint32_t addr;
        control_t control;
        bool forwarding;

        std::string toString() const
        {
            std::ostringstream oss;
            oss << "pc:" << pc
                << "|read_data_1:" << read_data_1
                << "|read_data_2:" << read_data_2
                << "|imm:" << imm
                << "|rt:" << rt
                << "|rd:" << rd
                << "|opcode:" << opcode
                << "|funct:" << funct
                << "|shamt:" << shamt
                << "|addr:" << addr
                << "|control:" << control;
            return oss.str();
        }
    };

    struct EX_MEM
    {
        uint32_t branch_target;
        uint32_t alu_zero;
        uint32_t alu_out;
        uint32_t write_data_mem;
        int write_reg;
        uint32_t addr;
        uint32_t branch_reg;
        control_t control;

        std::string toString() const
        {
            std::ostringstream oss;
            oss << "branch_target:" << branch_target
                << "|alu_zero:" << alu_zero
                << "|alu_out:" << alu_out
                << "|write_data_mem:" << write_data_mem
                << "|write_reg:" << write_reg
                << "|addr:" << addr
                << "|branch_reg:" << branch_reg
                << "|control:" << control;
            return oss.str();
        }
    };

    struct MEM_WB
    {
        uint32_t read_data_mem;
        uint32_t alu_out;
        int write_reg;
        control_t control;

        std::string toString() const
        {
            std::ostringstream oss;
            oss << "read_data_mem:" << read_data_mem
                << "|alu_out:" << alu_out
                << "|write_reg:" << write_reg
                << "|control:" << control;
            return oss.str();
        }
    };


        //two structs for reg in and reg out
        //set reg out = reg in when stepping. dont set when stalling
        //stage funcs modify reg in, processor step sets reg out
        IF_ID if_id_in, if_id_out;
        ID_EX id_ex_in, id_ex_out;
        EX_MEM ex_mem_in, ex_mem_out;
        MEM_WB mem_wb_in, mem_wb_out;
        std::queue<int> hazard_regs_queue;
        std::set<int> hazard_regs_set;
        bool flush_pipeline = false;

        // pipelined processor
        void fetch_stage();
        void decode_stage();
        void execute_stage();
        void memory_stage();
        void writeback_stage();
        // add private functions
        void single_cycle_processor_advance();
        void pipelined_processor_advance();
        
        void pop_hazard_regs();
        void push_hazard_regs(int _reg);
 
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
