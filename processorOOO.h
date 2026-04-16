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
        };

        struct ID_RN {
            uint32_t pc;

            uint8_t rs, rt, rd;
            uint32_t imm;
            uint32_t read_data_1, read_data_2;
            uint8_t shamt;
            int addr;

            uint8_t opcode, funct;

            control_t control;
        };

        struct RN_IS {
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
        };

        struct IS_DP {
            uint32_t pc;

            uint32_t operand_1;
            uint32_t operand_2;

            int phys_rd;

            control_t control;

            int rob_index;
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
        IF_ID if_id;
        ID_RN id_rn;
        RN_IS rn_is;
        IS_DP is_dp;
        DP_EX dp_ex;
        EX_WB ex_wb;
        WB_CM wb_cm;

        ReorderBuffer rob;
        PhysicalRegisterFile prf;
        InstructionQueue iq;

        bool flush_pipeline = false;
        bool stall = false;
        uint32_t pc_history[6] = {0};

        // OOO stage functions
        void fetch_stage();
        void decode_stage();
        void rename_stage();
        void issue_stage();
        void dispatch_stage();
        void execute_stage();
        void writeback_stage();
        void commit_stage();

    public:
        ProcessorOOO(Memory *mem)
        {
            regfile.pc = 0;
            memory = mem;

            memset(&if_id, 0, sizeof(if_id));
            memset(&id_rn, 0, sizeof(id_rn));
            memset(&rn_is, 0, sizeof(rn_is));
            memset(&is_dp, 0, sizeof(is_dp));
            memset(&dp_ex, 0, sizeof(dp_ex));
            memset(&ex_wb, 0, sizeof(ex_wb));
            memset(&wb_cm, 0, sizeof(wb_cm));

        }

        uint32_t getPC() { return opt_level == 0 ? regfile.pc : pc_history[0]; }
        void printRegFile() { regfile.print(); }
        void initialize(int opt_level);
        void advance();
        void out_of_order_advance();
};

#endif
