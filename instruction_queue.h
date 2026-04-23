#ifndef INSTRUCTION_QUEUE_H
#define INSTRUCTION_QUEUE_H

#include <iostream>
#include <vector>
#include <cstdint>
#include <string>   
#include <sstream>  
#include <iomanip>   

#include "config.h"
#include "control.h"

struct iq_instr
{
    uint64_t seq;
    uint32_t pc;
    int opcode;
    int rs;
    int rt;
    int rd;
    int shamt;
    int funct;
    uint32_t imm;
    int addr;
    control_t control;

    int rob_index; // Index in the reorder buffer
    int lsq_index;
    bool rs_ready;
    bool rt_ready;
};

class InstructionQueue
{
    private:
        std::vector<iq_instr> iq;
        std::vector<int> get_source_regs(iq_instr instr);
    public:
        bool full();
        bool empty() const { return iq.empty(); }
        bool add(iq_instr instr);
        bool remove(int index);
        int get_oldest_ready();
        void broadcast_ready(int phys_reg);
        void squash(uint64_t branch_seq);
        iq_instr get(int index);

        // Returns the relevant IQ state as a formatted string
        std::string to_string() const {
            std::stringstream ss;
            ss << "========== INSTRUCTION QUEUE (IQ) STATE ==========\n";
            ss << "Current Size: " << iq.size() << "\n\n";

            if (iq.empty()) {
                ss << "[ IQ is Empty ]\n";
            } else {
                ss << "Idx | Seq | PC       | RS (Rdy) | RT (Rdy) | RD  | ROB Idx\n";
                ss << "------------------------------------------------------------\n";
                
                for (size_t i = 0; i < iq.size(); ++i) {
                    const auto& inst = iq[i];
                    
                    ss << " ";
                    if (i < 10) ss << "0"; // basic padding
                    
                    ss << i << " | " 
                       << inst.seq << "   | "
                       << "0x" << std::hex << inst.pc << std::dec << " | "
                       << "P" << inst.rs << " (" << (inst.rs_ready ? "T" : "F") << ") | "
                       << "P" << inst.rt << " (" << (inst.rt_ready ? "T" : "F") << ") | "
                       << "P" << inst.rd << " | "
                       << inst.rob_index << "\n";
                }
            }
            ss << "============================================================\n";
            
            return ss.str();
        }

        // Prints the contents of the Instruction Queue
        void print() const {
            std::cout << to_string();
        }
};

#endif