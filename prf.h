#ifndef PRF_H
#define PRF_H

#include <vector>
#include <cstdint>
#include <iostream>
#include <deque>
#include "config.h"
#include "regfile.h"

class PhysicalRegisterFile {

    private:
        std::vector<PhysReg> R;
        std::vector<int> commited_regmap;
        std::vector<int> regmap; //arch -> phys register mappings
        std::deque<int> rename_pool; //free list 
    public:
        uint32_t pc;
        PhysicalRegisterFile() {
            R.resize(config::NUM_PHYSICAL_REGISTERS); 
            regmap.resize(config::NUM_ARCHITECTURAL_REGISTERS); 
            for (int i = 0; i < config::NUM_PHYSICAL_REGISTERS; i++) {
                R[i].value = 0;
                R[i].ready = true;
            }

            for(int i = 0; i < config::NUM_ARCHITECTURAL_REGISTERS; i++) {
                regmap[i] = i;
            }

            for(int i = config::NUM_ARCHITECTURAL_REGISTERS; i < config::NUM_PHYSICAL_REGISTERS; i++) { //remaning arch regs go on the free list
                rename_pool.push_back(i);
            }

        }
        bool ready(int reg) {
            return R[reg].ready;
        }

        uint32_t read(int phys_reg) {
            return R[phys_reg].value;
        }

        void write(int phys_reg, uint32_t result) {
            R[phys_reg].value = result;
            R[phys_reg].ready = true;
        }
        
        int assign_mapping(int arch_reg) {
            int phys_reg = rename_pool.front();
            rename_pool.pop_front();
            regmap[arch_reg] = phys_reg;

            R[phys_reg].ready = false;

            return phys_reg;
        }

        int get_mapping(int arch_reg) {
            return regmap[arch_reg];
        }

        void reclaim(int phys_reg) {
            rename_pool.push_back(phys_reg);
        }

        void undo_rename(int phys_reg) {
            rename_pool.push_front(phys_reg);
        }

        void rollback_rename(int arch_reg, int new_phys_reg, int old_phys_reg) {
            rename_pool.push_front(new_phys_reg);
            regmap[arch_reg] = old_phys_reg;
        }

        void update_commited_registers(int arch_reg, int phys_reg) {
            commited_regmap[arch_reg] = phys_reg;
        }

        bool has_free_phys_reg() {
            return rename_pool.size() > 0;
        }

        // Prints the contents of all the registers
        void print() {
            for(int i = 0; i < config::NUM_PHYSICAL_REGISTERS; ++i) {
                std::cout << std::dec << "R[" << i << "]: " << R[i].value << "\n";
            }
        }
        // Prints the contents of the register specified by reg 
        // This function should help you debug your code
        void print(int reg) {
            std::cout << "R[" << reg << "]: " << R[reg].value << "\n";
        }

};

#endif
