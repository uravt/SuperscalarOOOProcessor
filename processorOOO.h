#ifndef PROCESSOR_OOO_H
#define PROCESSOR_OOO_H

#include "memory.h"
#include "regfile.h"
#include "ALU.h"
#include "control.h"
#include <sstream>
#include <queue>
#include <vector>

class ProcessorOOO {
    private:
        int opt_level;
        ALU alu;
        control_t control;
        Memory *memory;
        Registers regfile;

        // Pipeline registers for OOO: Fetch -> Decode -> Rename -> Issue -> Dispatch -> Execute -> Writeback -> Commit

    struct IF_ID
    {
    };

    struct ID_RN
    {
    };

    struct RN_IS
    {
    };

    struct IS_DP
    {
    };

    struct DP_EX
    {
    };

    struct EX_WB
    {
    };

    struct WB_CM
    {
    };

    // Pipeline register instances (in/out pairs)
    IF_ID if_id_in, if_id_out;
    ID_RN id_rn_in, id_rn_out;
    RN_IS rn_is_in, rn_is_out;
    IS_DP is_dp_in, is_dp_out;
    DP_EX dp_ex_in, dp_ex_out;
    EX_WB ex_wb_in, ex_wb_out;
    WB_CM wb_cm_in, wb_cm_out;

    bool flush_pipeline = false;
    bool stall = false;
    uint32_t pc_history[8] = {0};

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

            memset(&if_id_in, 0, sizeof(if_id_in));
            memset(&if_id_out, 0, sizeof(if_id_out));
            memset(&id_rn_in, 0, sizeof(id_rn_in));
            memset(&id_rn_out, 0, sizeof(id_rn_out));
            memset(&rn_is_in, 0, sizeof(rn_is_in));
            memset(&rn_is_out, 0, sizeof(rn_is_out));
            memset(&is_dp_in, 0, sizeof(is_dp_in));
            memset(&is_dp_out, 0, sizeof(is_dp_out));
            memset(&dp_ex_in, 0, sizeof(dp_ex_in));
            memset(&dp_ex_out, 0, sizeof(dp_ex_out));
            memset(&ex_wb_in, 0, sizeof(ex_wb_in));
            memset(&ex_wb_out, 0, sizeof(ex_wb_out));
            memset(&wb_cm_in, 0, sizeof(wb_cm_in));
            memset(&wb_cm_out, 0, sizeof(wb_cm_out));
        }

        uint32_t getPC() { return opt_level == 0 ? regfile.pc : pc_history[0]; }
        void printRegFile() { regfile.print(); }
        void initialize(int opt_level);
        void advance();
        void out_of_order_advance();
};

#endif
