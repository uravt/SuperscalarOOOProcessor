#ifndef MEMORY_OOO_H
#define MEMORY_OOO_H

#include <vector>
#include <cstdint>
#include <iostream>
#include <cmath>
#include <string>

#include "config.h"
#include "memory.h"  // for CacheLine + CACHE_LINE_SIZE

struct NBCacheMSHR
{
    uint32_t block_addr;
    int countdown;
    bool valid;
};

class NBCacheLevel
{
private:
    std::vector<CacheLine> line;
    std::vector<NBCacheMSHR> mshrs;
    int size;
    int assoc;
    int missPenalty;
    std::string name;

    int findMSHR(uint32_t block_addr) const;
    int allocateMSHR(uint32_t block_addr);

public:
    NBCacheLevel(std::string nm, int sz, int asc, int penalty)
    {
        name = nm;
        size = sz;
        assoc = asc;
        missPenalty = penalty;
        line.resize(size / CACHE_LINE_SIZE);
        for (int i = 0; i < (int)line.size(); i++) line[i].valid = false;
        mshrs.resize(config::NUM_MSHRS);
        for (auto &m : mshrs) m.valid = false;
    }

    int getOffset(uint32_t address) const { return address & (CACHE_LINE_SIZE - 1); }
    int getIndex(uint32_t address) const { return (address >> (int)log2(CACHE_LINE_SIZE)) & (size / CACHE_LINE_SIZE - 1); }
    int getTag(uint32_t address) const { return address >> (int)log2(size); }
    uint32_t blockOf(uint32_t address) const { return address & ~(uint32_t)(CACHE_LINE_SIZE - 1); }

    bool isHit(uint32_t address, uint32_t &loc);
    void updateReplacementBits(int idx, int way);

    bool read(uint32_t address, uint32_t &read_data);
    bool write(uint32_t address, uint32_t write_data);

    CacheLine readLine(uint32_t address);
    void writeBackLine(CacheLine evictedLine);
    void replace(uint32_t address, CacheLine newLine, CacheLine &evictedLine);
    void invalidateLine(uint32_t address);

    bool blockInFlight(uint32_t address) const { return findMSHR(blockOf(address)) != -1; }
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

    bool access(uint32_t address, uint32_t &read_data, uint32_t write_data, bool mem_read, bool mem_write);

    void print(uint32_t address, int num_words)
    {
        for (uint32_t i = address; i < address + num_words; ++i)
        {
            std::cout << "MEM[" << std::hex << i << "]: " << mem[i] << std::dec << "\n";
        }
    }
};

#endif
