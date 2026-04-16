#include "reorder_buffer.h"

ReorderBuffer::ReorderBuffer() {
    size = 0;
    head = 0;
    tail = 0;
}

int ReorderBuffer::insert(int dest_arch_reg, int dest_phys_reg, int old_phys_reg) {
    if (isFull()) {
        return -1; // Buffer is full we should stall
    }
    int rob_index = tail;
    buffer[tail] = {dest_arch_reg, dest_phys_reg, old_phys_reg, false};
    tail = (tail + 1) % config::REORDER_BUFFER_SIZE;
    size++;
    return rob_index;
}

bool ReorderBuffer::commit(PhysicalRegisterFile& prf) {
    if (size > 0 && buffer[head].completed) {
        ROBEntry entry = buffer[head];
        head = (head + 1) % config::REORDER_BUFFER_SIZE;
        size--;

        if(entry.dest_arch_reg != 0) {
            prf.reclaim(entry.old_phys_reg); // Reclaim the old physical register
            prf.update_commited_registers(entry.dest_arch_reg, entry.dest_phys_reg); //update the retirement rat, allows us to recover from mispredictions
        }

        return true;
    }

    return false;
}

bool ReorderBuffer::isFull() {
    return size >= config::REORDER_BUFFER_SIZE;
}
