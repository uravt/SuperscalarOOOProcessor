#include "instruction_queue.h"
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
