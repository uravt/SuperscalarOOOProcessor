#include <cstdint>
#include <vector>
#include "memory.h"
struct MSHRTarget {
    int rob_index;
    int reg;
};

struct MSHREntry {
    uint32_t block_addr;
    bool valid;

    std::vector<MSHRTarget> targets;
};
struct CacheResponse {
    int rob_index;
    int reg;
    uint32_t data;
};
class NonBlockingCache
{
    private:
        Memory *memory;
        std::vector<MSHREntry> MSHRs;
        
    public:
        std::vector<CacheResponse> readyResponses;
        void initialize();
        bool allocateMSHR(uint32_t block_addr, int rob_index, int reg);
        bool findOpenMSHR(int &index);
        void checkReady();
};