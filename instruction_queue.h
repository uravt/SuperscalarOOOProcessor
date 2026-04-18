#ifndef INSTRUCTION_QUEUE_H
#define INSTRUCTION_QUEUE_H

#include <iostream>
#include <vector>
#include <cstdint>

#include "config.h"
#include "control.h"

struct iq_instr
{
    int opcode;
    int rs;
    int rt;
    int rd;
    int shamt;
    int funct;
    uint32_t imm;
    int addr;
    control_t control;

    bool ready;

    int rob_index; // Index in the reorder buffer
    bool rs_ready;
    bool rt_ready;
};

class InstructionQueue
{
    private:
        std::vector<iq_instr> iq;
        vector<int> getSourceRegs(iq_instr instr);
    public:
        bool add(iq_instr instr);
        bool remove(int index);
        bool isNonHazard(iq_instr reg);
        void readyDependicies();
        bool getOldestReady(iq_instr&, int &index);
        void broadcast_ready(int phys_reg)
};



#endif
