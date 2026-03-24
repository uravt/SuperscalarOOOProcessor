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
}

void Processor::advance() {
    switch (opt_level) {
        case 0: single_cycle_processor_advance();
                break;
        case 1: pipelined_processor_advance();
                break;
        // other optimization levels go here
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
    if(!cache_hit) { //cache miss -> need to stall
        memory_stage();
        return;//replaced with just return. memory just needs to rerun
    } 
    writeback_stage();
    memory_stage();



    execute_stage();
    decode_stage();
    fetch_stage();


    if_id_out = if_id_in;
    id_ex_out = id_ex_in;
    ex_mem_out = ex_mem_in;
    mem_wb_out = mem_wb_in;

    DEBUG(std::cout << if_id_out.toString() << "\n";)
    DEBUG(std::cout << id_ex_out.toString() << "\n";)
    DEBUG(std::cout << ex_mem_out.toString() << "\n";)
    DEBUG(std::cout << mem_wb_out.toString() << "\n";)
}

//pipeline registers
//values got based on diagram registers in and out
//data types based on single processor implementation



//arg input should be if_id_in?? return should also be if_id_in??
//im just going to use globals

//these methods are basically copy and paste from single cycle implentaion 
//but some inputs are from previous reg_out and outputs are put in the next reg_in
void Processor::fetch_stage() {
    // fetch
    if(flush_pipeline) {
        cout << "here" << "\n";
        memset(&if_id_in, 0, sizeof(if_id_in));

        flush_pipeline = false;
        return;
    }
    if(stall)
    {
        stall = false;
        return;
    }

    cache_hit = true;
    uint32_t instruction;
    cache_hit &= memory->access(regfile.pc, instruction, 0, 1, 0);
    DEBUG(cout << "\nPC: 0x" << std::hex << regfile.pc << std::dec << "\n");
    // increment pc
    
    if(!cache_hit) {
        return;
    }
    regfile.pc += 4;//WARNING: In single cycle PC is updated at the end. verify that when adding branching pc is not updated twice.

    if_id_in.instruction = instruction;
    if_id_in.pc = regfile.pc;
}



void Processor::decode_stage() {
    if(flush_pipeline) {
        memset(&id_ex_in, 0, sizeof(id_ex_in));
        return;
    }

    control.decode(if_id_out.instruction);
    DEBUG(if_id_out.toString());

    // extract rs, rt, rd, imm, funct 
    int opcode = (if_id_out.instruction >> 26) & 0x3f;
    int rs = (if_id_out.instruction >> 21) & 0x1f;
    int rt = (if_id_out.instruction >> 16) & 0x1f;
    int rd = (if_id_out.instruction >> 11) & 0x1f;
    int shamt = (if_id_out.instruction >> 6) & 0x1f;
    int funct = if_id_out.instruction & 0x3f;
    uint32_t imm = (if_id_out.instruction & 0xffff);
    cout << "imm: " << imm << "\n";
    cout << rt << "\n";
    int addr = if_id_out.instruction & 0x3ffffff;
    // Variables to read data into
    uint32_t read_data_1 = 0;
    uint32_t read_data_2 = 0;


    //stall check
    if (id_ex_out.control.mem_read &&
        (id_ex_out.rt == rs || id_ex_out.rt == rt)) 
    {
        stall = true;
        memset(&id_ex_in, 0, sizeof(id_ex_in));
        return;
    }
    // Read from reg file
    regfile.access(rs, rt, read_data_1, read_data_2, 0, 0, 0);
        cout << "read_data " << read_data_1 << "\n";
    id_ex_in.pc = if_id_out.pc;
    id_ex_in.read_data_1 = read_data_1;
    id_ex_in.read_data_2 = read_data_2;
    id_ex_in.imm = imm;
    id_ex_in.rt = rt;
    id_ex_in.rd = rd;
    id_ex_in.rs = rs;
    id_ex_in.shamt = shamt;
    id_ex_in.funct = funct;
    id_ex_in.addr = addr;
    id_ex_in.control = control;
    id_ex_in.opcode = opcode;
}



void Processor::execute_stage() {
    // Execution 
    alu.generate_control_inputs(id_ex_out.control.ALU_op, id_ex_out.funct, id_ex_out.opcode);
   
    // Sign Extend Or Zero Extend the immediate
    // Using Arithmetic right shift in order to replicate 1 
    uint32_t imm = id_ex_out.control.zero_extend ? id_ex_out.imm : (id_ex_out.imm >> 15) ? 0xffff0000 | id_ex_out.imm : id_ex_out.imm;
    
    // Find operands for the ALU Execution
    // Operand 1 is always R[rs] -> read_data_1, except sll and srl
    // Operand 2 is immediate if ALU_src = 1, for I-type
    uint32_t operand_1 = id_ex_out.control.shift ? id_ex_out.shamt : id_ex_out.read_data_1;
    uint32_t operand_2 = id_ex_out.control.ALU_src ? imm : id_ex_out.read_data_2;
    uint32_t alu_zero = 0;


    //FORWARDING

    //MEM
    if(mem_wb_out.control.reg_write && mem_wb_out.write_reg != 0)
    {
        uint32_t wb_val =
            mem_wb_out.control.mem_to_reg ?
            mem_wb_out.read_data_mem :
            mem_wb_out.alu_out;

        if(mem_wb_out.write_reg == id_ex_out.rs)
        {
            DEBUG(printf("Fowarding from wb, rs %d, operand_1 = %d\n",id_ex_out.rs,wb_val);)
            operand_1 = wb_val;
        }


        if(mem_wb_out.write_reg == id_ex_out.rt && !id_ex_out.control.ALU_src)
        {
            DEBUG(printf("Fowarding from wb, rt %d, operand_2 = %d\n",id_ex_out.rt,wb_val);)
            operand_2 = wb_val;
        }

    }
    //EX
    if(ex_mem_out.control.reg_write && ex_mem_out.write_reg != 0)
    {
        if(ex_mem_out.write_reg == id_ex_out.rs)
        {
            operand_1 = ex_mem_out.alu_out;
            DEBUG(printf("Fowarding from mem, rs %d, operand_1 = %d\n",id_ex_out.rs,ex_mem_out.alu_out);)
        }


        if(ex_mem_out.write_reg == id_ex_out.rt && !id_ex_out.control.ALU_src)
        {
            operand_2 = ex_mem_out.alu_out;
            DEBUG(printf("Fowarding from mem, rt %d, operand_2 = %d\n",id_ex_out.rt,ex_mem_out.alu_out);)
        }
    }
    uint32_t store_data = id_ex_out.read_data_2;

    // Forward into store data (rt)
    if (ex_mem_out.control.reg_write && ex_mem_out.write_reg != 0) {
        if (ex_mem_out.write_reg == id_ex_out.rt) {
            store_data = ex_mem_out.alu_out;
        }
    }
    if (mem_wb_out.control.reg_write && mem_wb_out.write_reg != 0) {
        uint32_t wb_val =
            mem_wb_out.control.mem_to_reg ?
            mem_wb_out.read_data_mem :
            mem_wb_out.alu_out;
        if (mem_wb_out.write_reg == id_ex_out.rt) {
            store_data = wb_val;
        }
    }


    DEBUG(printf("operand 1: %d | operand 2: %d\n", operand_1, operand_2);)

    uint32_t alu_result = alu.execute(operand_1, operand_2, alu_zero);

    bool branch_taken = id_ex_out.control.branch && 
    ((id_ex_out.control.bne && !alu_zero) || 
    (!id_ex_out.control.bne && alu_zero));

    bool jump = id_ex_out.control.jump || id_ex_out.control.jump_reg;
    
    if(branch_taken || jump) {
        if(branch_taken) {
            regfile.pc = id_ex_out.pc + (imm << 2);
        }
        else if(id_ex_out.control.jump) {
            regfile.pc = (id_ex_out.pc & 0xf0000000) | (id_ex_out.addr << 2);
        } 
        else {
            regfile.pc = id_ex_out.read_data_1;
        }

        flush_pipeline = true;
    }

    ex_mem_in.branch_target = id_ex_out.pc + (imm << 2); //NOTE: accurate???
    ex_mem_in.alu_zero = alu_zero;
    ex_mem_in.alu_out = alu_result;
    ex_mem_in.write_data_mem = store_data;
    ex_mem_in.write_reg = id_ex_out.control.reg_dest ? id_ex_out.rd : id_ex_out.rt;//from end of single cycle.
    ex_mem_in.addr = id_ex_out.addr;
    ex_mem_in.branch_reg = id_ex_out.read_data_1;
    ex_mem_in.control = id_ex_out.control;

    ex_mem_in.rt = id_ex_out.rt;
    ex_mem_in.rd = id_ex_out.rd;


}

void Processor::memory_stage() {
    uint32_t read_data_mem = 0;
    uint32_t write_data_mem = 0;

    cache_hit = true;
    // Memory
    // First read no matter whether it is a load or a store
    cache_hit &= memory->access(ex_mem_out.alu_out, read_data_mem, 0, ex_mem_out.control.mem_read  | ex_mem_out.control.mem_write , 0);
    // Stores: sb or sh mask and preserve original leftmost bits
    /*write_data_mem = control.halfword ? (read_data_mem & 0xffff0000) | (read_data_2 & 0xffff) : 
                    control.byte ? (read_data_mem & 0xffffff00) | (read_data_2 & 0xff): read_data_2;*/
    write_data_mem = ex_mem_out.write_data_mem;
    // Write to memory only if mem_write is 1, i.e store
    cache_hit &= memory->access(ex_mem_out.alu_out, read_data_mem, write_data_mem, ex_mem_out.control.mem_read , ex_mem_out.control.mem_write);
    // Loads: lbu or lhu modify read data by masking
    read_data_mem &= ex_mem_out.control.halfword ? 0xffff : ex_mem_out.control.byte ? 0xff : 0xffffffff;

    int write_reg = ex_mem_out.control.link ? 31 : ex_mem_out.write_reg;

    uint32_t write_data = ex_mem_out.control.link ? regfile.pc+8 : ex_mem_out.control.mem_to_reg ? read_data_mem : ex_mem_out.alu_out;  //NOTE: unused?

    /*
    regfile.pc += (control.branch && !control.bne && alu_zero) || (control.bne && !alu_zero) ? imm << 2 : 0; 
    regfile.pc = control.jump_reg ? read_data_1 : control.jump ? (regfile.pc & 0xf0000000) & (addr << 2): regfile.pc;
    */
    cout << cache_hit << "\n";
    if(!cache_hit) {
        return;
    }
    
    mem_wb_in.read_data_mem = read_data_mem;
    mem_wb_in.alu_out = ex_mem_out.alu_out;
    mem_wb_in.write_reg = ex_mem_out.write_reg;
    mem_wb_in.control = ex_mem_out.control;

    mem_wb_in.rt = ex_mem_out.rt;
    mem_wb_in.rd = ex_mem_out.rd;

}

void Processor::writeback_stage() {
    uint32_t read_data_2 = 0; //NOTE: i think the single cycle reused a variable. shouldnt matter as this access is just a write
    uint32_t write_data = mem_wb_out.control.mem_to_reg ?  mem_wb_out.read_data_mem : mem_wb_out.alu_out;
    regfile.access(0, 0, read_data_2, read_data_2, mem_wb_out.write_reg, mem_wb_out.control.reg_write, write_data);

}

