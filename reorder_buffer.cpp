#include "reorder_buffer.h"

ReorderBuffer::ReorderBuffer() {
    size = 0;
    head = 0;
    tail = 0;
}

int ReorderBuffer::insert(uint64_t seq, int dest_arch_reg, int dest_phys_reg, int old_phys_reg) {
    if (full()) {
        return -1; // Buffer is full we should stall
    }
    int rob_index = tail;
    buffer[tail] = {seq, dest_arch_reg, dest_phys_reg, old_phys_reg, false};
    tail = (tail + 1) % config::REORDER_BUFFER_SIZE;
    size++;
    return rob_index;
}

void ReorderBuffer::squash(uint64_t branch_seq, PhysicalRegisterFile& prf) {
    while (size > 0) {
        int prev = (tail - 1 + config::REORDER_BUFFER_SIZE) % config::REORDER_BUFFER_SIZE;
        ROBEntry &entry = buffer[prev];
        if (entry.seq <= branch_seq) break;
        if (entry.dest_phys_reg != entry.old_phys_reg) {
            prf.rollback_rename(entry.dest_arch_reg, entry.dest_phys_reg, entry.old_phys_reg);
        }
        tail = prev;
        size--;
    }
}

bool ReorderBuffer::commit(PhysicalRegisterFile& prf, LoadStoreQueue &lsq) {
    if (size > 0 && buffer[head].completed) {
        ROBEntry entry = buffer[head];
        head = (head + 1) % config::REORDER_BUFFER_SIZE;
        size--;

        if(entry.dest_arch_reg != 0) {
            if(entry.lsq_index != -1) {
                lsq.get_load(entry.lsq_index).completed = true;
            }
            prf.reclaim(entry.old_phys_reg); // Reclaim the old physical register
            prf.update_commited_registers(entry.dest_arch_reg, entry.dest_phys_reg); //update the retirement rat, allows us to recover from mispredictions
        }

        return true;
    }

    return false;
}

void ReorderBuffer::set_ready(int index) {
    buffer[index].completed = true;
}

bool ReorderBuffer::full() {
    return size >= config::REORDER_BUFFER_SIZE;
}
