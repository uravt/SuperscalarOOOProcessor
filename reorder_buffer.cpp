#include "reorder_buffer.h"
#include "load_store_queue.h"

ReorderBuffer::ReorderBuffer() {
    size = 0;
    head = 0;
    tail = 0;
}

int ReorderBuffer::insert(uint64_t seq, int dest_arch_reg, int dest_phys_reg, int old_phys_reg, int load_index, int store_index) {
    if (full()) {
        return -1; // Buffer is full we should stall
    }
    int rob_index = tail;
    buffer[tail] = {seq, dest_arch_reg, dest_phys_reg, old_phys_reg, load_index, store_index, false};
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

        if(entry.dest_phys_reg != entry.old_phys_reg) {
            prf.reclaim(entry.old_phys_reg); // Reclaim the old physical register
            prf.update_commited_registers(entry.dest_arch_reg, entry.dest_phys_reg); //update the retirement rat, allows us to recover from mispredictions
        }

        if(entry.load_index != -1) {
            // Load already wrote its result via execute/memory stage; pop from the LQ.
            lsq.pop_load_head();
        }
        if(entry.store_index != -1) {
            // Store hasn't touched memory yet — mark it ready to drain.
            // evict_commited_stores will pop it and write to memory in order.
            lsq.mark_store_complete(entry.seq);
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
