#ifndef LOAD_STORE_QUEUE_H
#define LOAD_STORE_QUEUE_H

#include <iostream>
#include <array>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>

#include "config.h"
#include "prf.h"

struct LoadEntry
{
    uint64_t seq;
    uint32_t pc;
    int rob_index;

    uint32_t addr;
    bool addr_ready;

    bool completed;

    int dest_phys_reg;

    bool byte;
    bool halfword;

    bool issued;
};

struct StoreEntry
{
    uint64_t seq;
    uint32_t pc;
    int rob_index;

    uint32_t addr;
    bool addr_ready;

    int src;
    int src_ready; 

    bool byte;
    bool halfword;

    bool issued;
    bool completed;
};

class LoadStoreQueue
{
private:
    std::array<LoadEntry, config::LOAD_QUEUE_SIZE> lq;
    int lq_size;
    int lq_head;
    int lq_tail;

    std::array<StoreEntry, config::STORE_QUEUE_SIZE> sq;
    int sq_size;
    int sq_head;
    int sq_tail;

public:
    LoadStoreQueue();

    bool lq_full() const { return lq_size >= config::LOAD_QUEUE_SIZE; }
    bool sq_full() const { return sq_size >= config::STORE_QUEUE_SIZE; }
    bool lq_empty() const { return lq_size == 0; }
    bool sq_empty() const { return sq_size == 0; }

    int add_load(const LoadEntry &e);
    int add_store(const StoreEntry &e);

    int find_load_by_seq(uint64_t seq) const;
    int find_store_by_seq(uint64_t seq) const;

    LoadEntry &get_load(int index) { return lq[index]; }
    StoreEntry &get_store(int index) { return sq[index]; }

    void set_load_address(uint64_t seq, uint32_t addr);
    void set_store_address(uint64_t seq, uint32_t addr);
    void set_store_data(uint64_t seq, uint32_t data, PhysicalRegisterFile &prf);

    void mark_load_issued(uint64_t seq);
    void mark_store_issued(uint64_t seq);
    void mark_load_complete(uint64_t seq);
    void mark_store_complete(uint64_t seq);

    int oldest_ready_load() const;
    int oldest_ready_store() const;

    bool try_forward(uint64_t load_seq, uint32_t &value, PhysicalRegisterFile &prf) const;
    bool has_unresolved_earlier_store(uint64_t load_seq) const;

    bool load_head_completed() const;
    bool store_head_completed() const;
    void pop_load_head();
    void pop_store_head();

    void squash(uint64_t branch_seq);
    
    void broadcast_ready(int phys_reg);

    std::string to_string() const
    {
        std::stringstream ss;
        ss << "========== LOAD/STORE QUEUE (LSQ) STATE ==========\n";

        ss << "-- Load Queue --  size: " << lq_size << " / " << config::LOAD_QUEUE_SIZE
           << " | head: " << lq_head << " | tail: " << lq_tail << "\n";
        if (lq_size == 0)
        {
            ss << "[ LQ is Empty ]\n";
        }
        else
        {
            ss << "Idx | Seq | PC       | Addr (Rdy) | Iss | Cmp | RD  | ROB\n";
            ss << "-----------------------------------------------------------\n";
            for (int i = 0; i < lq_size; ++i)
            {
                int idx = (lq_head + i) % config::LOAD_QUEUE_SIZE;
                const auto &e = lq[idx];
                ss << " " << std::setw(2) << std::setfill('0') << idx << std::setfill(' ')
                   << " | " << e.seq
                   << "   | 0x" << std::hex << e.pc << std::dec
                   << " | 0x" << std::hex << e.addr << std::dec
                   << " (" << (e.addr_ready ? "T" : "F") << ") | "
                   << (e.issued ? "T" : "F") << "   | "
                   << (e.completed ? "T" : "F") << "   | P"
                   << e.dest_phys_reg << " | "
                   << e.rob_index << "\n";
            }
        }

        ss << "\n-- Store Queue -- size: " << sq_size << " / " << config::STORE_QUEUE_SIZE
           << " | head: " << sq_head << " | tail: " << sq_tail << "\n";
        if (sq_size == 0)
        {
            ss << "[ SQ is Empty ]\n";
        }
        else
        {
            ss << "Idx | Seq | PC       | Addr (Rdy) | Data (Rdy) | Iss | Cmp | ROB\n";
            ss << "----------------------------------------------------------------\n";
            for (int i = 0; i < sq_size; ++i)
            {
                int idx = (sq_head + i) % config::STORE_QUEUE_SIZE;
                const auto &e = sq[idx];
                ss << " " << std::setw(2) << std::setfill('0') << idx << std::setfill(' ')
                   << " | " << e.seq
                   << "   | 0x" << std::hex << e.pc << std::dec
                   << " | 0x" << std::hex << e.addr << std::dec
                   << " (" << (e.addr_ready ? "T" : "F") << ") | "
                   << e.src
                   << " (" << (e.src_ready ? "T" : "F") << ") | "
                   << (e.issued ? "T" : "F") << "   | "
                   << (e.completed ? "T" : "F") << "   | "
                   << e.rob_index << "\n";
            }
        }

        ss << "==================================================================\n";
        return ss.str();
    }

    void print() const { std::cout << to_string(); }
};

#endif
