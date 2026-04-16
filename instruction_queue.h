#ifndef INSTRUCTION_QUEUE_H
#define INSTRUCTION_QUEUE_H

#include <iostream>
#include <queue>
#include <cstdint>
#include <array>

#include "config.h"

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

    bool ready;

    int rob_index; // Index in the reorder buffer
    bool valid;
};

class InstructionQueue
{
    private:
        std::array<iq_instr, config::INSTRUCTION_QUEUE_SIZE> iq;
    public:
        bool add(iq_instr instr);
        void pop();
        bool isHazard(int reg);
        void readyDependicies(int reg);

};



#endif
