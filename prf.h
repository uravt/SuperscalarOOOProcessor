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
            R.resize(config::NUM_PHYSICAL_REGISTERS); //assuming 96 physical registers
            for (int i = 0; i < config::NUM_PHYSICAL_REGISTERS; i++) {
                R[i].value = 0;
                R[i].ready = true;
            }

            regmap.resize(config::NUM_ARCHITECTURAL_REGISTERS); //assuming 32 architectural registers
            for(int i = 0; i < config::NUM_PHYSICAL_REGISTERS; i++) {
                rename_pool.push_back(i);
            }

        }
        // read_reg_1, read_reg_2 are register numbers from which the data should be read
        // read_data_1, read_data_2 are variables into which the data is read. These are passed by reference
        // write_reg is the register number to which the data needs to be written
        // write is a flag which incidates whether it is a write access or not
        // write_data is the data which needs to be written to write_reg
        void access(int read_reg_1, int read_reg_2, uint32_t &read_data_1, uint32_t &read_data_2,
                int write_reg, bool write, uint32_t write_data) {
            read_data_1 = R[read_reg_1].value;
            read_data_2 = R[read_reg_2].value;
            if (write) {
                R[write_reg].value = write_data;
                R[write_reg].ready = true;
            }
        }

        bool ready(int reg) {
            return R[reg].ready;
        }
        
        int assign_mapping(int arch_reg) {
            int phys_reg = rename_pool[0];
            rename_pool.pop_front();
            regmap[arch_reg] = phys_reg;

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

        void update_commited_registers(int arch_reg, int phys_reg) {
            commited_regmap[arch_reg] = phys_reg;
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
