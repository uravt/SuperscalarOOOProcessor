#include "instruction_queue.h"
<<<<<<< Updated upstream

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
=======
void InstructionQueue::push(iq_instr instr)
{
    iq.push(instr);
    instr.ready = true;
}
/* void InstructionQueue::pop()
{
    iq_instr removedElement = iq.front();
    iq.pop();
    iv.erase(std::remove(iv.begin(), iv.end(), removedElement), iv.end());
} */
bool InstructionQueue::isHazard(int reg)
{
    std::queue<iq_instr> temp = iq;
    while(!temp.empty())
    {
        iq_instr instr = temp.front();
        temp.pop();
        if(instr.rs == reg || instr.rt == reg || instr.rd == reg)
        {
            instr.ready = false;
            return true;
        }
    }
    return false;
}
void InstructionQueue::readyDependicies(int reg)
{
    std::queue<iq_instr> temp = iq;
    while(!temp.empty())
    {
        iq_instr instr = temp.front();
        temp.pop();
        if(instr.rs == reg || instr.rt == reg || instr.rd == reg)
        {
            instr.ready = true;
        }
    }
}
>>>>>>> Stashed changes
