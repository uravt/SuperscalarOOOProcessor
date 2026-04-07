#include <vector>
#include <cstdint>
#include <iostream>
#include <deque>
#include "prf.h"
#include "config.h"

struct ROBEntry {
    int dest_arch_reg;
    int dest_phys_reg;
    int old_phys_reg;
    bool completed;
}

class ReorderBuffer {
    private:
        std::deque<ROBEntry> buffer;
        int size;
    public:
        ReorderBuffer() {
            size = 0; 
        }

        bool insert(int dest_arch_reg, int dest_phys_reg, int old_phys_reg) {
            if (isFull()) {
                return false; // Buffer is full
            }
            buffer.push_back({dest_arch_reg, dest_phys_reg, old_phys_reg, false});
            size++;
            return true;
        }

        void commit(&PhysicalRegisterFile prf) {
            while(!buffer.empty() && buffer.front().completed) {
                ROBEntry entry = buffer.front();
                buffer.pop_front();
                size--;

                prf.reclaim(entry.old_phys_reg); // Reclaim the old physical register
            }
        }

        bool isFull() {
            return size >= ROB_SIZE;
        }
}