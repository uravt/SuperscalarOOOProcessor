#ifndef FUNCTIONAL_UNITS_H
#define FUNCTIONAL_UNITS_H

#include "ALU.h"
#include "instruction_queue.h"
#include "config.h"

#include <iostream>
#include <string>    // Added
#include <sstream>   // Added
#include <iomanip>   // Added for hex formatting

struct FunctionalUnit
{
    bool ready;
    iq_instr instr;
    ALU alu;

    bool has_result;
    iq_instr result_instr;
    uint32_t result;
};


class FunctionalUnits {
    private:
        FunctionalUnit units[config::NUM_ALUS];
    public:
        FunctionalUnits() {
            for(int i = 0; i < config::NUM_ALUS; i++) {
                units[i].ready = true;
                units[i].has_result = false;
            }
        }
        bool full() const {
            for(int i = 0; i < config::NUM_ALUS; i++) {
                if(units[i].ready) {
                    return false;
                }
            }
            return true;
        }
        bool empty() const {
            for(int i = 0; i < config::NUM_ALUS; i++) {
                if(!units[i].ready || units[i].has_result) {
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
        FunctionalUnit& get(int index) {
            return units[index];
        }
        void squash(uint64_t branch_seq) {
            for(int i = 0; i < config::NUM_ALUS; i++) {
                if(!units[i].ready && units[i].instr.seq > branch_seq) {
                    units[i].ready = true;
                }
                if(units[i].has_result && units[i].result_instr.seq > branch_seq) {
                    units[i].has_result = false;
                }
            }
        }

        // Returns the relevant Functional Units state as a formatted string
        std::string to_string() const {
            std::stringstream ss;
            ss << "========== FUNCTIONAL UNITS STATE ==========\n";
            ss << "Total ALUs: " << config::NUM_ALUS << "\n\n";

            ss << "Unit | Exec Seq | Exec PC  | WB Seq | WB PC    | Result\n";
            ss << "-------------------------------------------------------\n";

            for(int i = 0; i < config::NUM_ALUS; ++i) {
                const auto& unit = units[i];

                ss << "  " << i << "  | ";

                if (unit.ready) {
                    ss << "  ---   | -------- | ";
                } else {
                    ss << std::setw(6) << unit.instr.seq << "  | "
                       << "0x" << std::setw(6) << std::left << std::hex << unit.instr.pc << std::right << std::dec << " | ";
                }

                if (unit.has_result) {
                    ss << std::setw(4) << unit.result_instr.seq << "   | "
                       << "0x" << std::setw(6) << std::left << std::hex << unit.result_instr.pc << std::right << std::dec << " | "
                       << unit.result << "\n";
                } else {
                    ss << " ---   | -------- | ------\n";
                }
            }
            ss << "===================================================\n";
            
            return ss.str();
        }

        // Prints the contents of the Functional Units
        void print() const {
            std::cout << to_string();
        }
};

#endif