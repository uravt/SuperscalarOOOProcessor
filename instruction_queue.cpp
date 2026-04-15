#include "instruction_queue.h"

bool InstructionQueue::add(iq_instr instr)
{
    for(int i = 0; i < config::INSTRUCTION_QUEUE_SIZE; i++)
    {
        if(!iq[i].valid)
        {
            iq[i] = instr;
            iq[i].valid = true;
            return true;
        }
    }
    return false; // queue full -> stall
}

void InstructionQueue::pop()
{
    // TODO: implement pop semantics (shift entries / mark invalid)
}
