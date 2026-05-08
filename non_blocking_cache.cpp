#include "non_blocking_cache.h"
#include "config.h"
void NonBlockingCache::initialize()
{
    MSHRs.resize(config::NUM_MSHRS);

    for (auto &m : MSHRs)
    {
        m.valid = false;
    }
}

bool NonBlockingCache::allocateMSHR(uint32_t block_addr, int rob_index, int reg)
{
    
    for (int i = 0; i < (int) MSHRs.size(); i++)//Existing allocation spotted
    {
        if (MSHRs[i].valid && MSHRs[i].block_addr == block_addr)
        {
            MSHRs[i].targets.push_back({rob_index, reg});
            return true;
        }
    }
    int index;
    if(findOpenMSHR(index))//new allocation
    {
        MSHRs[index].targets.clear();
        MSHRs[index].block_addr = block_addr;
        MSHRs[index].valid = true;
        MSHRs[index].targets.push_back({rob_index,reg});
        return true;
    }
    else//no space
    {
        return false;
    }
}
bool NonBlockingCache::findOpenMSHR(int &index)
{
    for(int i = 0; i < (int) MSHRs.size(); i++)
    {
        if(!MSHRs.at(i).valid)
        {
            index = i;
            return true;
        }
    }
    return false;
}
void NonBlockingCache::checkReady()
{
    for (int i = 0; i < (int) MSHRs.size(); i++)
    {
        if (!MSHRs[i].valid) continue;

        uint32_t data = 0;

        bool done = memory->access(
            MSHRs[i].block_addr,
            data,
            0,    // write_data
            true, // mem_read
            false // mem_write
        );

        if (done)
        {
            for (auto &m : MSHRs[i].targets)
            {
                readyResponses.push_back({m.rob_index ,m.reg ,data});
            }

            MSHRs[i].valid = false;
        }
    }
}