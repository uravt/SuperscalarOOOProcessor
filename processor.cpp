#include <cstdint>
#include <cstring>
#include <cstdio>
#include <iostream>
#include "processor.h"

using namespace std;

#define ENABLE_DEBUG
#ifdef ENABLE_DEBUG
#define DEBUG(x) x
#else
#define DEBUG(x) 
#endif

void Processor::initialize(int level) {
    // Initialize Control
    control = {.reg_dest = 0, 
               .jump = 0,
               .jump_reg = 0,
               .link = 0,
               .shift = 0,
               .branch = 0,
               .bne = 0,
               .mem_read = 0,
               .mem_to_reg = 0,
               .ALU_op = 0,
               .mem_write = 0,
               .halfword = 0,
               .byte = 0,
               .ALU_src = 0,
               .reg_write = 0,
               .zero_extend = 0};
   
    opt_level = level;
    // Optimization level-specific initialization
    if (opt_level == 2) {
        ooo.initialize(level);
    }
}

void Processor::advance() {
    switch (opt_level) {
        case 0: single_cycle_processor_advance();
                break;
        case 1: pipelined_processor_advance();
                break;
        // other optimization levels go here
        case 2: ooo.out_of_order_advance();
                break;
        default: break;
    }
}

void Processor::single_cycle_processor_advance() {
    // fetch
    uint32_t instruction;
    memory->access(regfile.pc, instruction, 0, 1, 0);
    DEBUG(cout << "\nPC: 0x" << std::hex << regfile.pc << std::dec << "\n");
    // increment pc
    regfile.pc += 4;
    
    // decode into contol signals
    control.decode(instruction);
    DEBUG(control.print());

    // extract rs, rt, rd, imm, funct 
    int opcode = (instruction >> 26) & 0x3f;
    int rs = (instruction >> 21) & 0x1f;
    int rt = (instruction >> 16) & 0x1f;
    int rd = (instruction >> 11) & 0x1f;
    int shamt = (instruction >> 6) & 0x1f;
    int funct = instruction & 0x3f;
    uint32_t imm = (instruction & 0xffff);
    int addr = instruction & 0x3ffffff;
    // Variables to read data into
    uint32_t read_data_1 = 0;
    uint32_t read_data_2 = 0;
    
    // Read from reg file
    regfile.access(rs, rt, read_data_1, read_data_2, 0, 0, 0);
    
    // Execution 
    alu.generate_control_inputs(control.ALU_op, funct, opcode);
   
    // Sign Extend Or Zero Extend the immediate
    // Using Arithmetic right shift in order to replicate 1 
    imm = control.zero_extend ? imm : (imm >> 15) ? 0xffff0000 | imm : imm;
    
    // Find operands for the ALU Execution
    // Operand 1 is always R[rs] -> read_data_1, except sll and srl
    // Operand 2 is immediate if ALU_src = 1, for I-type
    uint32_t operand_1 = control.shift ? shamt : read_data_1;
    uint32_t operand_2 = control.ALU_src ? imm : read_data_2;
    uint32_t alu_zero = 0;

    uint32_t alu_result = alu.execute(operand_1, operand_2, alu_zero);
    
    
    uint32_t read_data_mem = 0;
    uint32_t write_data_mem = 0;

    // Memory
    // First read no matter whether it is a load or a store
    memory->access(alu_result, read_data_mem, 0, control.mem_read | control.mem_write, 0);
    // Stores: sb or sh mask and preserve original leftmost bits
    write_data_mem = control.halfword ? (read_data_mem & 0xffff0000) | (read_data_2 & 0xffff) : 
                    control.byte ? (read_data_mem & 0xffffff00) | (read_data_2 & 0xff): read_data_2;
    // Write to memory only if mem_write is 1, i.e store
    memory->access(alu_result, read_data_mem, write_data_mem, control.mem_read, control.mem_write);
    // Loads: lbu or lhu modify read data by masking
    read_data_mem &= control.halfword ? 0xffff : control.byte ? 0xff : 0xffffffff;

    int write_reg = control.link ? 31 : control.reg_dest ? rd : rt;

    uint32_t write_data = control.link ? regfile.pc+8 : control.mem_to_reg ? read_data_mem : alu_result;  

    // Write Back
    regfile.access(0, 0, read_data_2, read_data_2, write_reg, control.reg_write, write_data);
    
    // Update PC
    regfile.pc += (control.branch && !control.bne && alu_zero) || (control.bne && !alu_zero) ? imm << 2 : 0; 
    regfile.pc = control.jump_reg ? read_data_1 : control.jump ? (regfile.pc & 0xf0000000) & (addr << 2): regfile.pc;
}

void Processor::pipelined_processor_advance() {
    // pipelined processor logic goes here
    // does nothing currently -- if you call it from the cmd line, you'll run into an infinite loop
    // might be helpful to implement stages in a separate module
    mem_wb_prev = mem_wb; // snapshot for EX-stage MEM/WB forwarding before memory_stage overwrites it
    writeback_stage();
    memory_stage();
    execute_stage();
    decode_stage();
    fetch_stage();
    // Shift PC history — delays reported PC by pipeline depth
    // so the main loop doesn't exit before the pipeline drains
    for (int i = 0; i < 5; i++) pc_history[i] = pc_history[i+1];
    pc_history[5] = regfile.pc;
}

void Processor::fetch_stage() {
    // fetch
    if(load_use_stall || memory_stall) { 
        return;
    }

    uint32_t instruction;
    if(!memory->access(regfile.pc, instruction, 0, 1, 0)) { //cache miss on instr fetch, let's stall
        return;
    }   
    
    if_id.instruction = instruction;
    if_id.pc = regfile.pc;

    regfile.pc += 4;
}

void Processor::decode_stage() {
    if(memory_stall) {
        return;
    }

    load_use_stall = false; //reset load use stall

    control.decode(if_id.instruction);

    // extract rs, rt, rd, imm, funct 
    int opcode = (if_id.instruction >> 26) & 0x3f;
    int rs = (if_id.instruction >> 21) & 0x1f;
    int rt = (if_id.instruction >> 16) & 0x1f;
    int rd = (if_id.instruction >> 11) & 0x1f;
    int shamt = (if_id.instruction >> 6) & 0x1f;
    int funct = if_id.instruction & 0x3f;
    uint32_t imm = (if_id.instruction & 0xffff);
    int addr = if_id.instruction & 0x3ffffff;
    // Variables to read data into
    uint32_t read_data_1 = 0;
    uint32_t read_data_2 = 0;

    //load use hazard
    if(id_ex.control.mem_read && (id_ex.rt == rs || id_ex.rt == rt)) {
        id_ex = {};
        load_use_stall = true;
        return;
    } 

    // Read from reg file
    regfile.access(rs, rt, read_data_1, read_data_2, 0, 0, 0);
    id_ex.pc = if_id.pc;
    id_ex.read_data_1 = read_data_1;
    id_ex.read_data_2 = read_data_2;
    id_ex.imm = imm;
    id_ex.rt = rt;
    id_ex.rd = rd;
    id_ex.rs = rs;
    id_ex.shamt = shamt;
    id_ex.funct = funct;
    id_ex.addr = addr;
    id_ex.control = control;
    id_ex.opcode = opcode;
}



void Processor::execute_stage() {
    if(memory_stall) {
        return;
    }
    // Execution 
    alu.generate_control_inputs(id_ex.control.ALU_op, id_ex.funct, id_ex.opcode);
   
    // Sign Extend Or Zero Extend the immediate
    // Using Arithmetic right shift in order to replicate 1 
    uint32_t imm = id_ex.control.zero_extend ? id_ex.imm : (id_ex.imm >> 15) ? 0xffff0000 | id_ex.imm : id_ex.imm;
    
    // Find operands for the ALU Execution
    // Operand 1 is always R[rs] -> read_data_1, except sll and srl
    // Operand 2 is immediate if ALU_src = 1, for I-type

    int mem_dest = mem_wb_prev.control.reg_dest ? mem_wb_prev.rd : mem_wb_prev.rt;
    if(mem_wb_prev.control.reg_write && mem_dest != 0 && mem_dest == id_ex.rs) {
        id_ex.read_data_1 = mem_wb_prev.control.mem_read ? mem_wb_prev.read_data_mem : mem_wb_prev.alu_out;
    }
    if(mem_wb_prev.control.reg_write && mem_dest != 0 && mem_dest == id_ex.rt) {
        id_ex.read_data_2 = mem_wb_prev.control.mem_read ? mem_wb_prev.read_data_mem : mem_wb_prev.alu_out;
    }
    
    int ex_dest = ex_mem.control.reg_dest ? ex_mem.rd : ex_mem.rt;
    if(ex_mem.control.reg_write && ex_dest!= 0 && ex_dest == id_ex.rs) {
        id_ex.read_data_1 = ex_mem.alu_out;
    }
    if(ex_mem.control.reg_write && ex_dest != 0 && ex_dest == id_ex.rt) {
        id_ex.read_data_2 = ex_mem.alu_out;
    }

    uint32_t operand_1 = id_ex.control.shift ? id_ex.shamt : id_ex.read_data_1;
    uint32_t operand_2 = id_ex.control.ALU_src ? imm : id_ex.read_data_2;
    uint32_t alu_zero = 0;
    uint32_t store_data = id_ex.read_data_2;

    uint32_t alu_result = alu.execute(operand_1, operand_2, alu_zero);

    bool branch_taken = id_ex.control.branch && 
    ((id_ex.control.bne && !alu_zero) || 
    (!id_ex.control.bne && alu_zero));

    bool jump = id_ex.control.jump || id_ex.control.jump_reg;
    
    if(branch_taken || jump) {
        if(branch_taken) {
            regfile.pc = id_ex.pc + 4 + (imm << 2);
        }
        else if(id_ex.control.jump) {
            regfile.pc = (id_ex.pc & 0xf0000000) | (id_ex.addr << 2);
        } 
        else {
            regfile.pc = id_ex.read_data_1;
        }

        flush_pipeline = true;
    }

    ex_mem.alu_zero = alu_zero;
    ex_mem.alu_out = alu_result;
    ex_mem.write_data_mem = store_data;
    ex_mem.write_reg = id_ex.control.reg_dest ? id_ex.rd : id_ex.rt;//from end of single cycle.
    ex_mem.addr = id_ex.addr;
    ex_mem.branch_reg = id_ex.read_data_1;
    ex_mem.control = id_ex.control;

    ex_mem.rt = id_ex.rt;
    ex_mem.rd = id_ex.rd;

    if(flush_pipeline) {
        flush();
        flush_pipeline = false;
    }
}

void Processor::memory_stage() {
    uint32_t read_data_mem = 0;
    uint32_t write_data_mem = 0;

    memory_stall = false;
    // Memory
    // First read no matter whether it is a load or a store, if not a success stall on miss
    memory_stall = !memory->access(ex_mem.alu_out, read_data_mem, 0, ex_mem.control.mem_read  | ex_mem.control.mem_write , 0);
    if(memory_stall) { //stall on cache miss
        return;
    }

    // Stores: sb or sh mask and preserve original leftmost bits
    /*write_data_mem = control.halfword ? (read_data_mem & 0xffff0000) | (read_data_2 & 0xffff) : 
                    control.byte ? (read_data_mem & 0xffffff00) | (read_data_2 & 0xff): read_data_2;*/
    write_data_mem = ex_mem.write_data_mem;
    // Write to memory only if mem_write is 1, i.e store
    memory->access(ex_mem.alu_out, read_data_mem, write_data_mem, ex_mem.control.mem_read , ex_mem.control.mem_write);
    // Loads: lbu or lhu modify read data by masking
    read_data_mem &= ex_mem.control.halfword ? 0xffff : ex_mem.control.byte ? 0xff : 0xffffffff;

    /*
    regfile.pc += (control.branch && !control.bne && alu_zero) || (control.bne && !alu_zero) ? imm << 2 : 0; 
    regfile.pc = control.jump_reg ? read_data_1 : control.jump ? (regfile.pc & 0xf0000000) & (addr << 2): regfile.pc;
    */
    
    mem_wb.read_data_mem = read_data_mem;
    mem_wb.alu_out = ex_mem.alu_out;
    mem_wb.write_reg = ex_mem.write_reg;
    mem_wb.control = ex_mem.control;

    mem_wb.rt = ex_mem.rt;
    mem_wb.rd = ex_mem.rd;

}

void Processor::writeback_stage() {
    uint32_t read_data_2 = 0; //NOTE: i think the single cycle reused a variable. shouldnt matter as this access is just a write
    uint32_t write_data = mem_wb.control.mem_to_reg ?  mem_wb.read_data_mem : mem_wb.alu_out;
    regfile.access(0, 0, read_data_2, read_data_2, mem_wb.write_reg, mem_wb.control.reg_write, write_data);

}

void Processor::flush() {
    if_id = {};
    id_ex = {};
}

#include <iostream>
#include <vector>
#include <map>
#include <iomanip>

class PipelineLogger {
private:
    // Keeps track of the order we fetched PCs in, so we can print rows in order
    std::vector<uint32_t> pc_order; 
    
    // Maps a PC to a vector of characters (the timeline). Index = cycle number.
    std::map<uint32_t, std::vector<char>> grid;
    
    int max_cycles = 0;

    void recordStage(uint32_t pc, int cycle, char stage) {
        // Assume PC 0 is a NOP/Bubble. Don't graph NOPs.
        if (pc == 0) return; 

        // If this is the first time we've seen this PC, initialize its row
        if (grid.find(pc) == grid.end()) {
            pc_order.push_back(pc);
            grid[pc] = std::vector<char>(500, ' '); // Pre-fill timeline with spaces
        }

        // Drop the stage character (F, D, E, M, W) into the correct cycle column
        if (cycle < (int) grid[pc].size()) {
            grid[pc][cycle] = stage;
        }
    }

public:
    void logCycle(int cycle, uint32_t if_pc, uint32_t id_pc, uint32_t ex_pc, uint32_t mem_pc, uint32_t wb_pc) {
        max_cycles = std::max(max_cycles, cycle);
        
        recordStage(if_pc, cycle, 'F');
        recordStage(id_pc, cycle, 'D');
        recordStage(ex_pc, cycle, 'E');
        recordStage(mem_pc, cycle, 'M');
        recordStage(wb_pc, cycle, 'W');
    }

    void printGrid() {
        std::cout << "\n=== PIPELINE SPACE-TIME DIAGRAM ===\n\n";
        
        // 1. Print the X-Axis (Cycle Numbers)
        std::cout << "  PC   | ";
        for (int i = 1; i <= max_cycles; i++) {
            std::cout << i % 10; // Print just the last digit to save horizontal space
        }
        std::cout << "\n-------+-";
        for (int i = 1; i <= max_cycles; i++) std::cout << "-";
        std::cout << "\n";

        // 2. Print the Y-Axis (PCs) and their timelines
        for (uint32_t pc : pc_order) {
            std::cout << "0x" << std::setfill('0') << std::setw(4) << std::hex << pc << " | ";
            for (int i = 1; i <= max_cycles; i++) {
                std::cout << grid[pc][i];
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }
};

