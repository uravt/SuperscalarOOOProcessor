#ifndef BRANCH_PREDICTOR_H
#define BRANCH_PREDICTOR_H

#include <cstdint>
#include <array>
#include <algorithm>
#include "config.h"

struct BTBEntry {
    uint32_t tag;
    uint32_t targetPC;
    bool valid;
};

struct BTBResult {
    bool hit;
    uint32_t targetPC;
};

class BranchPredictor {
    private:
        uint32_t global_history_register;
        std::array<uint8_t, (1 << config::NUM_GHR_BITS)> pattern_history_table;
        std::array<BTBEntry, config::BTB_SIZE> btb; 
        uint32_t history_mask = ~((~0) << config::NUM_GHR_BITS);

        // HELPER: Calculates the Gshare index to prevent destructive aliasing
        uint32_t get_pht_index(uint32_t pc) {
            // Shift PC by 2 to ignore word alignment, then XOR with history
            return ((pc >> 2) ^ global_history_register) & history_mask;
        }

    public:
        BranchPredictor() {
            global_history_register = 0;
            pattern_history_table.fill(1); // Initialize to weakly not taken

            for(auto& entry : btb) {
                entry.valid = false;
            }
        }

        // Called in the Instruction Fetch (IF) stage
        bool predictBranch(uint32_t pc) {
            uint32_t index = get_pht_index(pc);
            return pattern_history_table[index] >= 2; // 10 or 11 means Taken
        }

        // Called in the Execute (EX) or Memory (MEM) stage when branch actually resolves.
        // NOTE FOR PIPELINING: To be perfectly cycle-accurate, you should calculate the 'index' 
        // during predictBranch() in the IF stage, pass that specific uint32_t index down 
        // your pipeline registers, and use it here instead of calling get_pht_index(pc) again. 
        // This prevents state desync if the GHR changed while this branch was flowing down the pipe.
        void updatePredictor(uint32_t pc, bool taken) {
            uint32_t index = get_pht_index(pc); 

            // Update the 2-bit saturation counter
            if(taken) {
                pattern_history_table[index] = std::min<uint8_t>((uint8_t) 3, pattern_history_table[index] + 1);
            } else {
                pattern_history_table[index] = std::max<uint8_t>((uint8_t) 0, pattern_history_table[index] - 1);
            }

            // Update the global history shift register with the actual outcome
            global_history_register = ((global_history_register << 1) | taken) & history_mask;
        }

        BTBResult get_target(uint32_t pc) {
            uint32_t index = (pc >> 2) % config::BTB_SIZE;

            if(btb[index].valid && btb[index].tag == pc) {
                return {true, btb[index].targetPC};
            }
            return {false, 0};
        }

        // Updated to accept the 'taken' boolean
        void update_btb(uint32_t pc, uint32_t actual_target, bool taken) {
            // OPTIMIZATION: Only pollute the BTB with branches that are actually taken.
            // If it's not taken, the pipeline just falls through to PC + 4 naturally.
            if (!taken) return; 

            uint32_t index = (pc >> 2) % config::BTB_SIZE;

            btb[index].tag = pc;
            btb[index].targetPC = actual_target;
            btb[index].valid = true;
        }
};

#endif