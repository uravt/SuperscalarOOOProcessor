#include "memory_ooo.h"
#include <algorithm>

#ifdef ENABLE_DEBUG
#define DEBUG(x) x
#else
#define DEBUG(x)
#endif

using namespace std;

bool NBCacheLevel::isHit(uint32_t address, uint32_t &loc)
{
    int idx = getIndex(address);
    int tag = getTag(address);

    for (int w = 0; w < assoc; w++)
    {
        if (line[idx * assoc + w].valid && line[idx * assoc + w].tag == tag)
        {
            loc = idx * assoc + w;
            updateReplacementBits(idx, w);
            return true;
        }
    }
    return false;
}

void NBCacheLevel::updateReplacementBits(int idx, int way)
{
    uint8_t curRepl = line[idx * assoc + way].replBits;
    for (int w = 0; w < assoc; w++)
    {
        if (line[idx * assoc + w].valid && line[idx * assoc + w].replBits > curRepl)
            line[idx * assoc + w].replBits--;
    }
    line[idx * assoc + way].replBits = assoc - 1;
}

bool NBCacheLevel::read(uint32_t address, uint32_t &read_data)
{
    uint32_t loc = 0;
    if (!isHit(address, loc)) return false;
    read_data = line[loc].data[getOffset(address) / 4];
    DEBUG(cout << name + " (read hit): " << read_data << "<-[0x" << std::hex << address << std::dec << "]\n");
    return true;
}

bool NBCacheLevel::write(uint32_t address, uint32_t write_data)
{
    uint32_t loc = 0;
    if (!isHit(address, loc)) return false;
    line[loc].data[getOffset(address) / 4] = write_data;
    line[loc].dirty = true;
    DEBUG(cout << name + " (write hit): [0x" << std::hex << address << std::dec << "]<-" << write_data << "\n");
    return true;
}

CacheLine NBCacheLevel::readLine(uint32_t address)
{
    int idx = getIndex(address);
    int tag = getTag(address);

    for (int w = 0; w < assoc; w++)
    {
        if (line[idx * assoc + w].valid && line[idx * assoc + w].tag == tag)
        {
            return line[idx * assoc + w];
        }
    }
    CacheLine c;
    c.valid = false;
    return c;
}

void NBCacheLevel::writeBackLine(CacheLine evictedLine)
{
    int idx = getIndex(evictedLine.address);
    int tag = getTag(evictedLine.address);

    for (int w = 0; w < assoc; w++)
    {
        if (line[idx * assoc + w].valid && line[idx * assoc + w].tag == tag)
        {
            for (int i = 0; i < CACHE_LINE_SIZE / 4; i++)
            {
                line[idx * assoc + w].data[i] = evictedLine.data[i];
            }
            line[idx * assoc + w].dirty = true;
        }
    }
}

void NBCacheLevel::replace(uint32_t address, CacheLine newLine, CacheLine &evictedLine)
{
    int idx = getIndex(address);
    newLine.address = address;
    newLine.tag = getTag(address);
    newLine.valid = true;
    newLine.dirty = false;
    newLine.replBits = assoc - 1;

    for (int w = 0; w < assoc; w++)
    {
        if (line[idx * assoc + w].valid && line[idx * assoc + w].tag == newLine.tag)
        {
            return;
        }
    }

    int victim = 0;
    for (int w = 0; w < assoc; w++)
    {
        if (!line[idx * assoc + w].valid)
        {
            victim = w;
            break;
        }
        if (line[idx * assoc + w].replBits < line[idx * assoc + victim].replBits)
        {
            victim = w;
        }
    }

    DEBUG(cout << name + ": replacing line at idx:" << idx << " way:" << victim << " for 0x" << std::hex << address << std::dec << "\n");
    evictedLine = line[idx * assoc + victim];
    line[idx * assoc + victim] = newLine;
    updateReplacementBits(idx, victim);
}

void NBCacheLevel::invalidateLine(uint32_t address)
{
    int idx = getIndex(address);
    int tag = getTag(address);

    for (int w = 0; w < assoc; w++)
    {
        if (line[idx * assoc + w].valid && line[idx * assoc + w].tag == tag)
        {
            line[idx * assoc + w].valid = false;
        }
    }
}

void NonBlockingCache::initialize(int penalty)
{
    miss_penalty = penalty;
    MSHRs.resize(config::NUM_MSHRS);
    for (auto &m : MSHRs)
    {
        m.valid = false;
        m.fill_pending = false;
        m.targets.clear();
    }
}

bool NonBlockingCache::allocateMSHR(uint32_t addr, int rob_index, int reg, uint64_t seq)
{
    uint32_t block = addr & ~(uint32_t)(CACHE_LINE_SIZE - 1);

    for (auto &m : MSHRs)
    {
        if (m.valid && m.block_addr == block)
        {
            m.targets.push_back({rob_index, reg, addr, seq});
            return true;
        }
    }

    int index;
    if (!findOpenMSHR(index)) return false;

    MSHRs[index].block_addr = block;
    MSHRs[index].valid = true;
    MSHRs[index].countdown = miss_penalty - 1;
    MSHRs[index].fill_pending = false;
    MSHRs[index].targets.clear();
    MSHRs[index].targets.push_back({rob_index, reg, addr, seq});
    return true;
}

void NonBlockingCache::squash(uint64_t branch_seq)
{
    for (auto &m : MSHRs)
    {
        if (!m.valid) continue;
        m.targets.erase(
            std::remove_if(m.targets.begin(), m.targets.end(),
                           [branch_seq](const MSHRTarget &t) { return t.seq > branch_seq; }),
            m.targets.end());
        if (m.targets.empty())
        {
            m.valid = false;
            m.fill_pending = false;
        }
    }
}

bool NonBlockingCache::findOpenMSHR(int &index)
{
    for (int i = 0; i < (int)MSHRs.size(); i++)
    {
        if (!MSHRs[i].valid)
        {
            index = i;
            return true;
        }
    }
    return false;
}

void NonBlockingCache::checkReady()
{
    for (auto &m : MSHRs)
    {
        if (!m.valid) continue;

        if (!m.fill_pending)
        {
            if (m.countdown > 0)
            {
                m.countdown--;
                continue;
            }
            m.fill_pending = true;
        }

        // Latency elapsed — drive the line all the way into L1. MemoryOOO::access
        // walks one level per call (mem->L2, L2->L1, L1 hit), returning true only
        // on the final L1 hit. Loop until done so the fill resolves this cycle.
        uint32_t scratch = 0;
        bool done = false;
        for (int safety = 0; safety < 4 && !done; safety++)
        {
            done = memory->access(m.block_addr, scratch, 0, true, false);
        }
        if (!done) continue;

        for (auto &t : m.targets)
        {
            uint32_t word = 0;
            memory->access(t.addr, word, 0, true, false);
            readyResponses.push_back({t.rob_index, t.reg, word});
        }
        m.valid = false;
        m.fill_pending = false;
        m.targets.clear();
    }
}

bool MemoryOOO::access(uint32_t address, uint32_t &read_data, uint32_t write_data, bool mem_read, bool mem_write)
{
    // Match Memory's behavior: at -O0 and -O2 the cache hierarchy is bypassed,
    // so accesses always succeed in one cycle against backing storage. The OOO
    // pipeline (LSQ drain, SMC squash, NBC checkReady) was written against this
    // single-cycle contract; modeling MSHR latency here breaks those invariants.
    if (opt_level == 0)
    {
        if (mem_read)
            read_data = mem[address / 4];
        if (mem_write)
            mem[address / 4] = write_data;
        return true;
    }

    if (!mem_read && !mem_write)
        return true;

    if ((mem_read && L1.read(address, read_data)) || (mem_write && L1.write(address, write_data)))
    {
        return true;
    }
    else if ((mem_read && L2.read(address, read_data)) || (mem_write && L2.write(address, write_data)))
    {
        CacheLine evictedLine;
        L1.replace(address, L2.readLine(address), evictedLine);
        if (evictedLine.valid && evictedLine.dirty)
        {
            L2.writeBackLine(evictedLine);
        }
    }
    else
    {
        int lineAddr = address & ~(CACHE_LINE_SIZE - 1);
        CacheLine c;
        CacheLine evictedLine;
        evictedLine.valid = false;
        for (int i = 0; i < CACHE_LINE_SIZE / 4; i++)
        {
            c.data[i] = mem[lineAddr / 4 + i];
        }
        L2.replace(address, c, evictedLine);

        if (evictedLine.valid)
        {
            L1.invalidateLine(evictedLine.address);
        }
        if (evictedLine.valid && evictedLine.dirty)
        {
            lineAddr = evictedLine.address & ~(CACHE_LINE_SIZE - 1);
            for (int i = 0; i < CACHE_LINE_SIZE / 4; i++)
            {
                mem[lineAddr / 4 + i] = evictedLine.data[i];
            }
        }
    }
    return false;
}
