#ifndef REORDER_BUFFER_H
#define REORDER_BUFFER_H

#include <vector>
#include <cstdint>
#include <iostream>
#include <array>
#include <string>    
#include <sstream>   
#include "prf.h"
#include "config.h"

class LoadStoreQueue;

struct ROBEntry {
    uint64_t seq;
    int dest_arch_reg;
    int dest_phys_reg;
    int old_phys_reg;
    int load_index;
    int store_index;
    bool completed;
};

class ReorderBuffer {
    private:
        std::array<ROBEntry, config::REORDER_BUFFER_SIZE> buffer;
        int size;
        int head;
        int tail;
    public:
        ReorderBuffer();
        int insert(uint64_t seq, int dest_arch_reg, int dest_phys_reg, int old_phys_reg, int load_index, int store_index);
        bool commit(PhysicalRegisterFile& prf, LoadStoreQueue &lsq);
        bool full();
        bool empty() const { return size == 0; }
        void set_ready(int index);
        void squash(uint64_t branch_seq, PhysicalRegisterFile& prf);

        // Returns the relevant ROB state as a formatted string
        std::string to_string() const {
            std::stringstream ss;
            ss << "========== REORDER BUFFER (ROB) STATE ==========\n";
            ss << "Capacity: " << size << " / " << config::REORDER_BUFFER_SIZE << "\n";
            ss << "Head: " << head << " | Tail: " << tail << "\n\n";

            if (size == 0) {
                ss << "[ ROB is Empty ]\n";
            } else {
                ss << "Idx | Seq | Dest(Arch) | Dest(Phys) | Old(Phys) | Completed\n";
                ss << "-------------------------------------------------------------\n";
                
                // Iterate through the circular buffer
                for (int i = 0; i < size; ++i) {
                    int idx = (head + i) % config::REORDER_BUFFER_SIZE;
                    const auto& entry = buffer[idx];
                    
                    // Mark Head (H) and Last Inserted (T-1) for easier reading
                    ss << " ";
                    if (idx < 10) ss << "0"; // basic padding
                    ss << idx;
                    
                    if (idx == head) {
                        ss << "(H)";
                    } else if (idx == (head + size - 1) % config::REORDER_BUFFER_SIZE) {
                        ss << "(T)"; // technically Tail points to the next empty slot, so this is the last valid entry
                    } else {
                        ss << "   ";
                    }
                    
                    ss << " | " << entry.seq 
                       << "   | R" << entry.dest_arch_reg 
                       << "         | P" << entry.dest_phys_reg 
                       << "         | P" << entry.old_phys_reg 
                       << "        | " << (entry.completed ? "True" : "False") << "\n";
                }
            }
            ss << "==============================================================\n";
            
            return ss.str();
        }

        // Prints the contents of the ROB
        void print() const {
            std::cout << to_string();
        }
};

#endif