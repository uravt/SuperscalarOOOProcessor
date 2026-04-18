#include "instruction_queue.h"
#include <set>

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
bool InstructionQueue::remove(int index)
{
    if(index >= iq.size() || index < 0) return false;   
    iq.erase(iq.begin() + index);
    readyDependicies();
    return true;
}
bool InstructionQueue::isNonHazard(iq_instr instr) //checks if NEW instruction has a hazard ---- maybe can just run readyDependicies after adding for same effecy
{
    vector<int> sourceRegs = getSourceRegs(instr);
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
void InstructionQueue::readyDependicies() //recalculates ready instructions
{
    set<int> hazards;

    for(int i = 0; i < iq.size(); i++)//loop though iq from oldest to newest
    {
        iq.at(i).ready = true;
        vector<int> sourceRegs = getSourceRegs(iq.at(i));
        for(auto &sReg : sourceRegs)
        {
            if(hazards.count(sReg))//if sReg element is a hazard
            {
                iq.at(i).ready = false;
            }
        }

        if(iq.at(i).control.reg_write)
        {
            hazards.insert(iq.at(i).control.reg_dest);
        }
    }
}
vector<int> getSourceRegs(iq_instr instr)
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
}
bool InstructionQueue::getOldestReady(iq_instr &instr, int &index)
{
    bool valid = false;
    for(int i = 0; i < iq.size() ; i++)
    {
        if(iq.at(i).ready)
        {
            instr = iq.at(i);
            index = i;
            valid = true;
        }
    }
    return valid;
}
void InstructionQueue::broadcast_ready(int phys_reg) {
    for(auto &i : iq) {
        if(i.rs == phys_reg) {
            i.rs_ready = true;
        }
        if(i.rt == phys_reg) {
            i.rt_ready  = true;
        }
    }
}