#ifndef PRF_H
#define PRF_H

#include <vector>
#include <cstdint>
#include <iostream>
#include <deque>
#include <string>
#include <sstream>
#include "config.h"
#include "regfile.h"

class PhysicalRegisterFile
{

private:
    std::vector<PhysReg> R;
    std::vector<int> commited_regmap;
    std::vector<int> regmap;     // arch -> phys register mappings
    std::deque<int> rename_pool; // free list
public:
    uint32_t pc;
    PhysicalRegisterFile()
    {
        R.resize(config::NUM_PHYSICAL_REGISTERS);
        regmap.resize(config::NUM_ARCHITECTURAL_REGISTERS);
        commited_regmap.resize(config::NUM_ARCHITECTURAL_REGISTERS);
        for (int i = 0; i < config::NUM_PHYSICAL_REGISTERS; i++)
        {
            R[i].value = 0;
            R[i].ready = true;
        }

        for (int i = 0; i < config::NUM_ARCHITECTURAL_REGISTERS; i++)
        {
            regmap[i] = i;
            commited_regmap[i] = i;
        }

        for (int i = config::NUM_ARCHITECTURAL_REGISTERS; i < config::NUM_PHYSICAL_REGISTERS; i++)
        { // remaning arch regs go on the free list
            rename_pool.push_back(i);
        }
    }
    bool ready(int reg)
    {
        return R[reg].ready;
    }

    uint32_t read(int phys_reg)
    {
        return R[phys_reg].value;
    }

    void write(int phys_reg, uint32_t result)
    {
        R[phys_reg].value = result;
        R[phys_reg].ready = true;
    }

    int assign_mapping(int arch_reg)
    {
        int phys_reg = rename_pool.front();
        rename_pool.pop_front();
        regmap[arch_reg] = phys_reg;

        R[phys_reg].ready = false;

        return phys_reg;
    }

    int get_mapping(int arch_reg)
    {
        return regmap[arch_reg];
    }

    void reclaim(int phys_reg)
    {
        rename_pool.push_back(phys_reg);
    }

    void undo_rename(int phys_reg)
    {
        rename_pool.push_front(phys_reg);
    }

    void rollback_rename(int arch_reg, int new_phys_reg, int old_phys_reg)
    {
        rename_pool.push_front(new_phys_reg);
        regmap[arch_reg] = old_phys_reg;
    }

    void update_commited_registers(int arch_reg, int phys_reg)
    {
        commited_regmap[arch_reg] = phys_reg;
    }

    bool has_free_phys_reg()
    {
        return rename_pool.size() > 0;
    }

    // Returns all relevant PRF state as a formatted string
    std::string to_string() const
    {
        std::stringstream ss;
        ss << "========== PHYSICAL REGISTER FILE STATE ==========\n";
        ss << "PC: " << std::dec << pc << "\n\n";

        ss << "--- Speculative Map (Arch -> Phys) ---\n";
        for (int i = 0; i < config::NUM_ARCHITECTURAL_REGISTERS; ++i)
        {
            ss << "Arch R" << i << " -> Phys P" << regmap[i] << "\n";
        }
        ss << "\n";

        ss << "--- Committed Map (Arch -> Phys) ---\n";
        for (int i = 0; i < config::NUM_ARCHITECTURAL_REGISTERS; ++i)
        {
            ss << "Arch R" << i << " -> Phys P" << commited_regmap[i] << "\n";
        }
        ss << "\n";

        ss << "--- Free List (Rename Pool) ---\n";
        ss << "Size: " << rename_pool.size() << " | Free Registers: [ ";
        for (int reg : rename_pool)
        {
            ss << "P" << reg << " ";
        }
        ss << "]\n\n";

        ss << "--- Physical Registers ---\n";
        for (int i = 0; i < config::NUM_PHYSICAL_REGISTERS; ++i)
        {
            ss << "P[" << i << "]: Value = " << R[i].value
               << ", Ready = " << (R[i].ready ? "True" : "False") << "\n";
        }
        ss << "==================================================\n";

        return ss.str();
    }

    // Prints the complete state of the PRF
    void print_debug()
    {
        std::cout << to_string();
    }

    void print()
    {
        for (int i = 0; i < config::NUM_ARCHITECTURAL_REGISTERS; ++i)
        {
            std::cout << std::dec << "R[" << i << "]: " << R[(regmap[i])].value << "\n";
        }
    }

    // Prints the contents of a specific physical register
    void print(int reg)
    {
        std::cout << "P[" << reg << "]: Value = " << R[reg].value
                  << ", Ready = " << (R[reg].ready ? "True" : "False") << "\n";
    }
};

#endif