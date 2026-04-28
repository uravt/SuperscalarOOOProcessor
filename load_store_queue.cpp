#include "load_store_queue.h"
#include <stdio.h>

LoadStoreQueue::LoadStoreQueue()
    : lq_size(0), lq_head(0), lq_tail(0),
      sq_size(0), sq_head(0), sq_tail(0) {}

int LoadStoreQueue::add_load(const LoadEntry &e)
{
    if (lq_full()) return -1;
    int idx = lq_tail;
    lq[idx] = e;
    lq_tail = (lq_tail + 1) % config::LOAD_QUEUE_SIZE;
    lq_size++;
    return idx;
}

int LoadStoreQueue::add_store(const StoreEntry &e)
{
    if (sq_full()) return -1;
    int idx = sq_tail;
    sq[idx] = e;
    sq_tail = (sq_tail + 1) % config::STORE_QUEUE_SIZE;
    sq_size++;
    return idx;
}

int LoadStoreQueue::find_load_by_seq(uint64_t seq) const
{
    for (int i = 0; i < lq_size; i++)
    {
        int idx = (lq_head + i) % config::LOAD_QUEUE_SIZE;
        if (lq[idx].seq == seq) return idx;
    }
    return -1;
}

int LoadStoreQueue::find_store_by_seq(uint64_t seq) const
{
    for (int i = 0; i < sq_size; i++)
    {
        int idx = (sq_head + i) % config::STORE_QUEUE_SIZE;
        if (sq[idx].seq == seq) return idx;
    }
    return -1;
}

void LoadStoreQueue::set_load_address(uint64_t seq, uint32_t addr)
{
    int idx = find_load_by_seq(seq);
    if (idx == -1) return;
    lq[idx].addr = addr;
    lq[idx].addr_ready = true;
}

void LoadStoreQueue::set_store_address(uint64_t seq, uint32_t addr)
{
    int idx = find_store_by_seq(seq);
    if (idx == -1) return;
    sq[idx].addr = addr;
    sq[idx].addr_ready = true;
}

void LoadStoreQueue::mark_load_issued(uint64_t seq)
{
    int idx = find_load_by_seq(seq);
    if (idx != -1) lq[idx].issued = true;
}

void LoadStoreQueue::mark_store_issued(uint64_t seq)
{
    int idx = find_store_by_seq(seq);
    if (idx != -1) sq[idx].issued = true;
}

void LoadStoreQueue::mark_load_complete(uint64_t seq)
{
    int idx = find_load_by_seq(seq);
    if (idx == -1) return;
    lq[idx].completed = true;
}

void LoadStoreQueue::mark_store_complete(uint64_t seq)
{
    int idx = find_store_by_seq(seq);
    if (idx != -1) sq[idx].completed = true;
}

int LoadStoreQueue::oldest_ready_load() const
{
    for (int i = 0; i < lq_size; i++)
    {
        int idx = (lq_head + i) % config::LOAD_QUEUE_SIZE;
        const LoadEntry &l = lq[idx];
        if (l.issued || l.completed) continue;
        if (!l.addr_ready) continue;
        if (has_unresolved_earlier_store(l.seq)) continue;
        return idx;
    }
    return -1;
}

int LoadStoreQueue::oldest_ready_store() const
{
    if (sq_size == 0) return -1;
    const StoreEntry &h = sq[sq_head];
    if (!h.issued && !h.completed && h.addr_ready && h.src_ready) return sq_head;
    return -1;
}

bool LoadStoreQueue::try_forward(uint64_t load_seq, uint32_t &value, PhysicalRegisterFile &prf) const
{
    int load_idx = find_load_by_seq(load_seq);
    if (load_idx == -1 || !lq[load_idx].addr_ready) return false;

    uint32_t target_addr = lq[load_idx].addr;

    for (int i = sq_size - 1; i >= 0; i--)
    {
        int idx = (sq_head + i) % config::STORE_QUEUE_SIZE;
        const StoreEntry &s = sq[idx];
        if (s.seq >= load_seq) continue;
        if (!s.addr_ready || !s.src_ready) continue;
        if (s.addr != target_addr) continue;
        if (s.byte != lq[load_idx].byte) continue;
        if (s.halfword != lq[load_idx].halfword) continue;
        value = s.data;
        return true;
    }
    return false;
}

bool LoadStoreQueue::has_unresolved_earlier_store(uint64_t load_seq) const
{
    for (int i = 0; i < sq_size; i++)
    {
        int idx = (sq_head + i) % config::STORE_QUEUE_SIZE;
        const StoreEntry &s = sq[idx];
        if (s.seq >= load_seq) break;
        if (!s.addr_ready) return true;
    }
    return false;
}

bool LoadStoreQueue::load_head_completed() const
{
    return lq_size > 0 && lq[lq_head].completed;
}

bool LoadStoreQueue::store_head_completed() const
{
    return sq_size > 0 && sq[sq_head].completed;
}

void LoadStoreQueue::pop_load_head()
{
    if (lq_size == 0) return;
    lq_head = (lq_head + 1) % config::LOAD_QUEUE_SIZE;
    lq_size--;
}

void LoadStoreQueue::pop_store_head()
{
    if (sq_size == 0) return;
    sq_head = (sq_head + 1) % config::STORE_QUEUE_SIZE;
    sq_size--;
}

void LoadStoreQueue::squash(uint64_t branch_seq)
{
    while (lq_size > 0)
    {
        int prev = (lq_tail - 1 + config::LOAD_QUEUE_SIZE) % config::LOAD_QUEUE_SIZE;
        if (lq[prev].seq <= branch_seq) break;
        lq_tail = prev;
        lq_size--;
    }
    while (sq_size > 0)
    {
        int prev = (sq_tail - 1 + config::STORE_QUEUE_SIZE) % config::STORE_QUEUE_SIZE;
        if (sq[prev].seq <= branch_seq) break;
        sq_tail = prev;
        sq_size--;
    }
}

void LoadStoreQueue::broadcast_ready(int phys_reg) {
    for(auto &i : sq) {
        if(i.src == phys_reg) {
            i.src_ready = true;
        }
    }
}

LoadStoreQueue::EvictResult LoadStoreQueue::evict_commited_stores(MemoryOOO *mem,
        PhysicalRegisterFile &prf, uint32_t smc_text_end) {
    EvictResult result{false, 0, 0};
    while(!sq_empty() && sq[sq_head].completed) {
        uint32_t read_data = 0;
        StoreEntry &s = sq[sq_head];
        if(!mem->access(s.addr, read_data, s.data, false, true)) {
            break;
        }
        if (s.addr < smc_text_end &&
            (!result.smc_detected || s.seq < result.smc_seq)) {
            result.smc_detected = true;
            result.smc_seq = s.seq;
            result.smc_pc = s.pc;
        }
        pop_store_head();
    }
    return result;
}
