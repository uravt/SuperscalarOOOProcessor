#include "memory_ooo.h"

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

int NBCacheLevel::findMSHR(uint32_t block_addr) const
{
    for (int i = 0; i < (int)mshrs.size(); i++)
    {
        if (mshrs[i].valid && mshrs[i].block_addr == block_addr) return i;
    }
    return -1;
}

int NBCacheLevel::allocateMSHR(uint32_t block_addr)
{
    for (int i = 0; i < (int)mshrs.size(); i++)
    {
        if (!mshrs[i].valid)
        {
            mshrs[i].valid = true;
            mshrs[i].block_addr = block_addr;
            mshrs[i].countdown = missPenalty - 1;
            return i;
        }
    }
    return -1;
}

bool NBCacheLevel::read(uint32_t address, uint32_t &read_data)
{
    uint32_t loc = 0;
    uint32_t block = blockOf(address);
    int mshr_idx = findMSHR(block);

    if (mshr_idx != -1)
    {
        if (mshrs[mshr_idx].countdown > 0)
        {
            DEBUG(cout << name + " (read miss) at 0x" << std::hex << address << std::dec << ": " << mshrs[mshr_idx].countdown << " cycles left for block 0x" << std::hex << block << std::dec << "\n");
            mshrs[mshr_idx].countdown--;
            return false;
        }
        mshrs[mshr_idx].valid = false;
    }

    if (!isHit(address, loc))
    {
        if (allocateMSHR(block) == -1)
        {
            DEBUG(cout << name + " (read miss) at 0x" << std::hex << address << std::dec << ": MSHR pool full\n");
        }
        return false;
    }
    read_data = line[loc].data[getOffset(address) / 4];
    DEBUG(cout << name + " (read hit): " << read_data << "<-[0x" << std::hex << address << std::dec << "]\n");
    return true;
}

bool NBCacheLevel::write(uint32_t address, uint32_t write_data)
{
    uint32_t loc = 0;
    uint32_t block = blockOf(address);
    int mshr_idx = findMSHR(block);

    if (mshr_idx != -1)
    {
        if (mshrs[mshr_idx].countdown > 0)
        {
            DEBUG(cout << name + " (write miss) at 0x" << std::hex << address << std::dec << ": " << mshrs[mshr_idx].countdown << " cycles left for block 0x" << std::hex << block << std::dec << "\n");
            mshrs[mshr_idx].countdown--;
            return false;
        }
        mshrs[mshr_idx].valid = false;
    }

    if (!isHit(address, loc))
    {
        if (allocateMSHR(block) == -1)
        {
            DEBUG(cout << name + " (write miss) at 0x" << std::hex << address << std::dec << ": MSHR pool full\n");
        }
        return false;
    }
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

    for (int w = 0; w < assoc; w++)
    {
        if (line[idx * assoc + w].valid && line[idx * assoc + w].tag == newLine.tag)
        {
            return;
        }
    }
    for (int w = 0; w < assoc; w++)
    {
        if (!line[idx * assoc + w].valid || line[idx * assoc + w].replBits == 0)
        {
            DEBUG(cout << name + ": replacing line at idx:" << idx << " way:" << w << " for 0x" << std::hex << address << std::dec << "\n");
            evictedLine = line[idx * assoc + w];
            line[idx * assoc + w] = newLine;
            return;
        }
    }
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

bool MemoryOOO::access(uint32_t address, uint32_t &read_data, uint32_t write_data, bool mem_read, bool mem_write)
{
    if (opt_level == 0)
    {
        if (mem_read) read_data = mem[address / 4];
        if (mem_write) mem[address / 4] = write_data;
        return true;
    }

    if (!mem_read && !mem_write) return true;

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
