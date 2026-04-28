#ifndef MEMORY_OOO_H
#define MEMORY_OOO_H

#include <vector>
#include <cstdint>
#include <iostream>
#include <cmath>
#include <string>

#include "config.h"
#include "memory.h"  // for CacheLine + CACHE_LINE_SIZE

class NBCacheLevel
{
private:
    std::vector<CacheLine> line;
    int size;
    int assoc;
    int missPenalty;
    std::string name;

public:
    NBCacheLevel(std::string nm, int sz, int asc, int penalty)
    {
        name = nm;
        size = sz;
        assoc = asc;
        missPenalty = penalty;
        line.resize(size / CACHE_LINE_SIZE);
        for (int i = 0; i < (int)line.size(); i++) line[i].valid = false;
    }

    int getMissPenalty() const { return missPenalty; }

    int getOffset(uint32_t address) const { return address & (CACHE_LINE_SIZE - 1); }
    int getIndex(uint32_t address) const { return (address >> (int)log2(CACHE_LINE_SIZE)) & (size / (CACHE_LINE_SIZE * assoc) - 1); }
    int getTag(uint32_t address) const { return address >> (int)log2(size / assoc); }
    uint32_t blockOf(uint32_t address) const { return address & ~(uint32_t)(CACHE_LINE_SIZE - 1); }

    bool isHit(uint32_t address, uint32_t &loc);
    void updateReplacementBits(int idx, int way);

    bool read(uint32_t address, uint32_t &read_data);
    bool write(uint32_t address, uint32_t write_data);

    CacheLine readLine(uint32_t address);
    void writeBackLine(CacheLine evictedLine);
    void replace(uint32_t address, CacheLine newLine, CacheLine &evictedLine);
    void invalidateLine(uint32_t address);
};

class MemoryOOO
{
private:
    std::vector<uint32_t> mem;
    NBCacheLevel L1 = NBCacheLevel("L1-OOO", 32768, 8, 12);
    NBCacheLevel L2 = NBCacheLevel("L2-OOO", 262144, 8, 59);
    int opt_level;

public:
    MemoryOOO()
    {
        mem.resize(2097152, 0);
        opt_level = 0;
    }
    void setOptLevel(int level) { opt_level = level; }

    int getL1MissPenalty() const { return L1.getMissPenalty(); }

    // Side-effect-free L1 probe used by the issue stage so an L1 miss falls through
    // to the NBC (rather than `access` walking the hierarchy and burning L2 latency for free).
    bool probeL1(uint32_t address, uint32_t &read_data)
    {
        if (opt_level == 0)
        {
            read_data = mem[address / 4];
            return true;
        }
        return L1.read(address, read_data);
    }

    bool access(uint32_t address, uint32_t &read_data, uint32_t write_data, bool mem_read, bool mem_write);

    // Direct (no-cache) write into backing memory, used by the loader.
    void loadWord(uint32_t address, uint32_t word) { mem[address / 4] = word; }

    void print(uint32_t address, int num_words)
    {
        for (uint32_t i = address; i < address + num_words; ++i)
        {
            std::cout << "MEM[" << std::hex << i << "]: " << mem[i] << std::dec << "\n";
        }
    }
};

struct MSHRTarget
{
    int rob_index;
    int reg;
    uint32_t addr;   // full byte address — used to extract the right word from the filled line
    uint64_t seq;    // sequence number — used to drop targets squashed by a misprediction
};

struct MSHREntry
{
    uint32_t block_addr;
    bool valid;
    int countdown;       // cycles remaining until the line can be filled
    bool fill_pending;   // countdown hit zero; do the L2/mem walk on the next tick
    std::vector<MSHRTarget> targets;
};

struct CacheResponse
{
    int rob_index;
    int reg;
    uint32_t data;
};

class NonBlockingCache
{
private:
    std::vector<MSHREntry> MSHRs;
    int miss_penalty;

public:
    MemoryOOO *memory;
    std::vector<CacheResponse> readyResponses;

    void initialize(int penalty);
    bool allocateMSHR(uint32_t addr, int rob_index, int reg, uint64_t seq);
    bool findOpenMSHR(int &index);
    void checkReady();
    void squash(uint64_t branch_seq);
};

#endif
