#ifndef REORDER_BUFFER_H
#define REORDER_BUFFER_H

#include <vector>
#include <cstdint>
#include <iostream>
#include <array>
#include "prf.h"
#include "config.h"

struct ROBEntry {
    int dest_arch_reg;
    int dest_phys_reg;
    int old_phys_reg;
    bool completed;
};

class ReorderBuffer {
    private:
        std::array<ROBEntry, config::REORDER_BUFFER_SIZE> buffer;
        int size;
        int head;
        int tail;
    public:
        ReorderBuffer() {
            size = 0; 
            head = 0;
            tail = 0;
        }

        int insert(int dest_arch_reg, int dest_phys_reg, int old_phys_reg) {
            if (isFull()) {
                return -1; // Buffer is full we should stall
            }
            int rob_index = tail;
            buffer[tail] = {dest_arch_reg, dest_phys_reg, old_phys_reg, false};
            tail = (tail + 1) % config::REORDER_BUFFER_SIZE;
            size++;
            return rob_index;
        }

        void commit(PhysicalRegisterFile& prf) {
            int num_committed = 0;
            while(size > 0 && buffer[head].completed && num_committed < config::PIPELINE_WIDTH) {
                ROBEntry entry = buffer[head];
                head = (head + 1) % config::REORDER_BUFFER_SIZE;
                size--;
                num_committed++;
                
                prf.reclaim(entry.old_phys_reg); // Reclaim the old physical register
            }
        }

        bool isFull() {
            return size >= config::REORDER_BUFFER_SIZE;
        }
};

#endif