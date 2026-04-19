#include "ALU.h"
#include "instruction_queue.h"
#include "config.h"


struct FunctionalUnit
{
    bool ready;
    bool has_result;
    iq_instr instr; 
    ALU alu;
    uint32_t result;
};


class FunctionalUnits {
    private:
        FunctionalUnit units[config::NUM_ALUS];
    public:
        FunctionalUnits() {
            for(int i = 0; i < config::NUM_ALUS; i++) {
                units[i].ready  = true; 
            }
        }
        bool full() {
            for(int i = 0; i < config::NUM_ALUS; i++) {
                if(units[i].ready) {
                    return false;
                }
            }
            return true;
        }
        bool issue_to_unit(iq_instr instr) { 
            for(int i = 0; i < config::NUM_ALUS; i++) {
                if(units[i].ready) {
                    units[i].ready = false;
                    units[i].instr = instr;
                    return true;
                }
            }
            return false;
        }
        FunctionalUnit get(int index) {
            return units[index];
        }
};       
