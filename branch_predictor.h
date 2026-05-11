#include <array>
#include "config.h"

struct BTBEntry
{
    bool valid;
    int tag;
    int target;

    int bimodal_counter;
};

struct BTBResult 
{
    bool taken;
    int target;
};

class BranchPredictor
{
    private:
        std::array<BTBEntry, (1 << config::BTB_BITS)> btb;

    public:
        uint64_t total_branches = 0;
        uint64_t mispredictions = 0;

        BranchPredictor()
        {
            btb.fill({false, 0, 0, 0});
        }

        bool is_branch(int pc)
        {
            pc >>= 2;
            int index = ~(~(0) << config::BTB_BITS) & pc;
            int tag = (~(0) << config::BTB_BITS) & pc;

            return btb[index].valid && btb[index].tag == tag;
        }

        BTBResult predict(int pc) //check if branch first
        {
            // if(!is_branch(pc)) {
            //     return false;
            // } 

            pc >>= 2;
            int index = ~(~(0) << config::BTB_BITS) & pc;

            return {btb[index].bimodal_counter >= 2, btb[index].target};
        }

        void update(bool taken, int pc, int target)
        {
            pc >>= 2;
            int index = ~(~(0) << config::BTB_BITS) & pc;
            int tag = (~(0) << config::BTB_BITS) & pc;

            if(!btb[index].valid || btb[index].tag != tag) {
                btb[index].valid = true;
                btb[index].tag = tag;
                btb[index].bimodal_counter = taken ? 2 : 1;
                btb[index].target = target;
                return;
            }

            if(taken) {
                if(btb[index].bimodal_counter < 3)
                    btb[index].bimodal_counter++;
                btb[index].target = target;
            } else {
                if(btb[index].bimodal_counter > 0)
                    btb[index].bimodal_counter--;
            }
        }

        bool target_match(int pc, int target) {
            pc >>= 2;
            int index = ~(~(0) << config::BTB_BITS) & pc;
            int tag = (~(0) << config::BTB_BITS) & pc;

            return btb[index].valid && btb[index].tag == tag && btb[index].target == target;
        }
};