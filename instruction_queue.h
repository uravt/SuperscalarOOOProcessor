#ifndef INSTRUCTION_QUEUE_H
#define INSTRUCTION_QUEUE_H

#include <iostream>
#include <vector>
#include <cstdint>

#include "config.h"
#include "control.h"

struct iq_instr
{
    uint64_t seq;
    uint32_t pc;
    int opcode;
    int rs;
    int rt;
    int rd;
    int shamt;
    int funct;
    uint32_t imm;
    int addr;
    control_t control;

    int rob_index; // Index in the reorder buffer
    bool rs_ready;
    bool rt_ready;
};

class InstructionQueue
{
    private:
        std::vector<iq_instr> iq;
        std::vector<int> get_source_regs(iq_instr instr);
    public:
        bool full();
        bool add(iq_instr instr);
        bool remove(int index);
        int get_oldest_ready();
        void broadcast_ready(int phys_reg);
        void squash(uint64_t branch_seq);
        iq_instr get(int index);
};



#endif
