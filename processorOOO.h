#ifndef PROCESSOR_OOO_H
#define PROCESSOR_OOO_H

#include <sstream>
#include <queue>
#include <vector>

#include "memory.h"
#include "memory_ooo.h"
#include "regfile.h"
#include "ALU.h"
#include "control.h"
#include "reorder_buffer.h"
#include "instruction_queue.h"
#include "prf.h"
#include "functional_units.h"
#include "load_store_queue.h"
#include "branch_predictor.h"

class ProcessorOOO
{
private:
    int opt_level;
    ALU alu;
    control_t control;
    MemoryOOO *memory;

    // Pipeline registers for OOO: Fetch -> Decode -> Rename -> Issue -> Dispatch -> Execute -> Writeback -> Commit

    struct IF_ID
    {
        uint32_t pc;
        uint32_t instruction;
        bool in_use;

        bool branch_taken;
    };

    struct ID_RN
    {
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

        bool branch_taken;
    };

    std::string toString(const ID_RN &stage)
    {
        if (!stage.in_use)
            return "ID_RN: Not in use";

        std::stringstream ss;
        ss << "=== ID_RN Stage ===" << "\n";
        ss << "PC:         0x" << std::hex << std::setw(8) << std::setfill('0') << stage.pc << std::dec << "\n";
        ss << "Opcode:     0x" << std::hex << (int)stage.opcode << " | Funct: 0x" << (int)stage.funct << std::dec << "\n";

        ss << "Registers:  rs: " << (int)stage.rs
           << " (" << (stage.reads_rs ? "R" : "-") << "), "
           << "rt: " << (int)stage.rt
           << " (" << (stage.reads_rt ? "R" : "-") << "), "
           << "rd: " << (int)stage.rd << "\n";

        ss << "Data:       Read1: " << stage.read_data_1
           << " | Read2: " << stage.read_data_2 << "\n";

        ss << "Imm:        0x" << std::hex << stage.imm
           << " | Addr: 0x" << stage.addr << std::dec << "\n";

        ss << "Shamt:      " << (int)stage.shamt << "\n";

        // Note: This assumes your control_t has its own logic or you just want a placeholder
        ss << "Control:    [Mapped]";

        return ss.str();
    }

    // Pipeline register instances
    IF_ID if_id_buffer[config::PIPELINE_WIDTH];
    ID_RN id_rn_buffer[config::PIPELINE_WIDTH];

    ReorderBuffer rob;
    PhysicalRegisterFile prf;
    InstructionQueue iq;
    FunctionalUnits fu;
    LoadStoreQueue lsq;
    BranchPredictor bp;
    NonBlockingCache i_nbc;
    NonBlockingCache d_nbc;

    bool flush_pipeline = false;
    bool stall = false;
    uint32_t pc_history[6] = {0};

    uint64_t next_seq = 0;
    bool squash_pending = false;
    uint64_t squash_seq = 0;
    uint32_t squash_target_pc = 0;
    uint32_t end_pc = UINT32_MAX;

    int commited_instrs = 0;

    // OOO stage functions
    void fetch_stage();
    void decode_stage();
    void rename_stage();
    void dispatch_stage();
    void execute_stage();
    void memory_stage();
    void writeback_stage();
    void commit_stage();

    // helpers
    void update_reg_src_usage(int opcode, int funct, ID_RN &reg);
    void perform_squash();

public:
    ProcessorOOO(MemoryOOO *mem)
    {
        prf.pc = 0;
        memory = mem;
        for (int i = 0; i < config::PIPELINE_WIDTH; i++)
        {
            if_id_buffer[i].in_use = false;
            id_rn_buffer[i].in_use = false;
        }
    }

    uint32_t getPC()
    {
        // Keep main's bounds check satisfied until the pipeline is fully
        // drained: fetch past end-of-program AND every stage empty.
        if (prf.pc < end_pc)
            return 0;
        if (!rob.empty() || !fu.empty() || !lsq.sq_empty())
            return 0;
        for (int i = 0; i < config::PIPELINE_WIDTH; i++)
        {
            if (if_id_buffer[i].in_use || id_rn_buffer[i].in_use)
                return 0;
        }
        return UINT32_MAX;
    }
    void setEndPC(uint32_t e) { end_pc = e; }
    void printRegFile()
    {
        prf.print();
    }
    void initialize(int opt_level);
    void out_of_order_advance();
};

#endif
