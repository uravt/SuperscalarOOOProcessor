#ifndef PROCESSOR_OOO_H
#define PROCESSOR_OOO_H

#include <sstream>
#include <queue>
#include <vector>

#include "memory.h"
#include "regfile.h"
#include "ALU.h"
#include "control.h"
#include "reorder_buffer.h"
#include "instruction_queue.h"
#include "prf.h"
#include "functional_units.h"
#include "non_blocking_cache.h"


class ProcessorOOO {
    private:
        int opt_level;
        ALU alu;
        control_t control;
        Memory *memory;
        Registers regfile;

        // Pipeline registers for OOO: Fetch -> Decode -> Rename -> Issue -> Dispatch -> Execute -> Writeback -> Commit

        struct IF_ID {
            uint32_t pc;
            uint32_t instruction;
            bool in_use;
        };

        struct ID_RN {
            uint32_t pc;

            uint8_t rs, rt, rd;
            uint32_t imm;
            uint32_t read_data_1, read_data_2;
            uint8_t shamt;
            int addr;

            uint8_t opcode, funct;

            bool reads_rs, reads_rt;

            control_t control;
            bool in_use;
        };

        struct RN_DP {
            uint32_t pc;

            int phys_rs;
            int phys_rt;
            int phys_rd;

            bool rs_ready;
            bool rt_ready;

            uint32_t rs_val;
            uint32_t rt_val;

            control_t control;

            int rob_index;
            bool valid;
        };


        struct DP_EX {
            uint32_t operand_1;
            uint32_t operand_2;

            int phys_rd;

            control_t control;

            int rob_index;
        };

        struct EX_WB {
            uint32_t result;

            int phys_rd;

            control_t control;

            int rob_index;
        };

        struct WB_CM {
            uint32_t result;

            int phys_rd;
            int arch_rd;

            bool ready;

            int rob_index;
        };

        // Pipeline register instances
        IF_ID if_id_buffer[config::PIPELINE_WIDTH];
        ID_RN id_rn_buffer[config::PIPELINE_WIDTH];
        RN_DP rn_is_buffer[config::PIPELINE_WIDTH];
        DP_EX dp_ex_buffer[config::PIPELINE_WIDTH];
        EX_WB ex_wb_buffer[config::PIPELINE_WIDTH];
        WB_CM wb_cm_buffer[config::PIPELINE_WIDTH];

        ReorderBuffer rob;
        PhysicalRegisterFile prf;
        InstructionQueue iq;
        FunctionalUnits fu;
        NonBlockingCache i_nbc;
        NonBlockingCache d_nbc;

        bool flush_pipeline = false;
        bool stall = false;
        uint32_t pc_history[6] = {0};

        uint64_t next_seq = 0;
        bool squash_pending = false;
        uint64_t squash_seq = 0;
        uint32_t squash_target_pc = 0;

        // OOO stage functions
        void fetch_stage();
        void decode_stage();
        void rename_stage();
        void dispatch_stage();
        void execute_stage();
        void writeback_stage();
        void commit_stage();

        //helpers
        void update_reg_src_usage(int opcode, int funct, ID_RN &reg);
        void perform_squash();

    public:
        ProcessorOOO(Memory *mem)
        {
            regfile.pc = 0;
            memory = mem;
        }

        uint32_t getPC() { return opt_level == 0 ? regfile.pc : pc_history[0]; }
        void printRegFile() { regfile.print(); }
        void initialize(int opt_level);
        void advance();
        void out_of_order_advance();
};

#endif
