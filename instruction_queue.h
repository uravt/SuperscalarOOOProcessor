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
};

class InstructionQueue
{
    private:
        std::vector<iq_instr> iq;
    public:
        bool add(iq_instr instr);
        bool isNonHazard(iq_instr reg);
        void readyDependicies(int reg);
        bool getOldestReady(iq_instr&);
};



#endif
