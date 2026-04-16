#include "instruction_queue.h"

bool InstructionQueue::add(iq_instr instr)
{
    if(iq.size() >= config::INSTRUCTION_QUEUE_SIZE)
    {
        return false; // queue full -> stall
    }
    iq.push_back(instr);
    instr.ready = isNonHazard(instr);
    return true;
}
bool InstructionQueue::isNonHazard(iq_instr instr) //checks if NEW instruction has a hazard
{
    vector<int> sourceRegs;
    if(instr.opcode == 0 || instr.control.mem_write || instr.control.branch) // r-type or store or branch
    {
        sourceRegs.push_back(instr.rs);
        sourceRegs.push_back(instr.rt);
    }
    else if(instr.control.mem_read)
    {
        sourceRegs.push_back(instr.rs);
    }
    for (auto &i : iq)
    {
        if(i.control.reg_write)
        {
            for(auto &sReg : sourceRegs)
            {
                if(i.control.reg_dest == sReg)
                {
                    return false;
                }
            }
        }
    }
    return true;
}
void InstructionQueue::readyDependicies(int reg)
{
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
bool InstructionQueue::getOldestReady(iq_instr &instr)
{
    bool valid = false;
    for(auto &i : iq)
    {
        if(i.ready)
        {
            instr = i;
            valid = true;
        }
    }
    return valid;
}