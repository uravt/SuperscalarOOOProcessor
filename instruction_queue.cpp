#include "instruction_queue.h"
#include <set>

bool InstructionQueue::add(iq_instr instr)
{
    if(iq.size() >= config::INSTRUCTION_QUEUE_SIZE)
    {
        return false; // queue full -> stall
    }
    iq.push_back(instr);
    return true;
}
bool InstructionQueue::remove(int index)
{
    if(index >= iq.size() || index < 0) return false;   
    iq.erase(iq.begin() + index);
    return true;
}

std::vector<int> get_source_regs(iq_instr instr)
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
int InstructionQueue::get_oldest_ready()
{
    for(int i = 0; i < iq.size(); i++)
    {
        if(iq[i].rs_ready && iq[i].rt_ready)
        {
            return i;
        }
    }
    return -1;
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