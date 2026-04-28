#include "instruction_queue.h"
#include "load_store_queue.h"
#include <algorithm>
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
    if(index >= (int) iq.size() || index < 0) return false;   
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

    return sourceRegs;
}
int InstructionQueue::get_oldest_ready(LoadStoreQueue &lsq)
{
    for(int i = 0; i < (int) iq.size(); i++)
    {
        if(iq[i].rs_ready && iq[i].rt_ready && 
            (!iq[i].control.mem_read || !lsq.has_unresolved_earlier_store(iq[i].seq))) // don't issue a load if there are older unresolved stores
        {
            return i;
        }
    }
    return -1;
}
void InstructionQueue::squash(uint64_t branch_seq) {
    iq.erase(std::remove_if(iq.begin(), iq.end(),
        [branch_seq](const iq_instr &i) { return i.seq > branch_seq; }),
        iq.end());
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
iq_instr InstructionQueue::get(int index) {
    return iq.at(index);
}
bool InstructionQueue::full() {
    return iq.size() >= config::INSTRUCTION_QUEUE_SIZE;
}