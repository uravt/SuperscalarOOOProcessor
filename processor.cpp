#include <cstdint>
#include <iostream>
#include "processor.h"
using namespace std;

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
    
}

//pipeline registers
//values got based on diagram registers in and out
//data types based on single processor implementation
struct IF_ID
{
    uint32_t pc;
    uint32_t instruction
}
struct ID_EX
{
    uint32_t pc;
    uint32_t read_data_1;
    uint32_t read_data_2;
    uint32_t imm; //instruction[15-0]
    int rt; //instruction[20-16]
    int rd; //instruction[15-11]
    bool control_WB;
    bool control_M;
    bool control_EX
}
struct EX_MEM
{
    uint32_t branch_target; //NOTE: not found in single impentation?? 
    uint32_t alu_zero;
    uint32_t alu_out;
    uint32_t write_data_mem;
    int write_reg;
    bool control_WB;
    bool control_M;
}
struct MEM_WB
{
    uint32_t read_data_mem;
    uint32_t alu_out;
    int write_reg;
    bool control_WB;
}

//two structs for reg in and reg out
//set reg out = reg in when stepping. dont set when stalling
//stage funcs modify reg in, processor step sets reg out
IF_ID if_id_in, if_id_out;
ID_EX id_ex_in, id_ex_out;
EX_MEM ex_mem_in, ex_mem_out;
MEM_WB mem_wb_in, mem_wb_out;


//arg input should be if_id_in?? return should also be if_id_in??
//im just going to use globals

//these methods are basically copy and paste from single cycle implentaion 
//but some inputs are from previous reg_out and outputs are put in the next reg_in
void* fetch_stage(void* arg) {
    // fetch
    uint32_t instruction;
    memory->access(regfile.pc, instruction, 0, 1, 0);
    DEBUG(cout << "\nPC: 0x" << std::hex << regfile.pc << std::dec << "\n");
    // increment pc
    reg_file.pc += 4;

    if_id_in.instruction = instruction;
    if_id_in.pc = reg_file.pc;
    return NULL;//what do i return???
}



void* decode_stage(void* arg) {
    control.decode(if_id_out.instruction);
    DEBUG(control.print());

    // extract rs, rt, rd, imm, funct 
    int opcode = (if_id_out.instruction >> 26) & 0x3f;
    int rs = (if_id_out.instruction >> 21) & 0x1f;
    int rt = (if_id_out.instruction >> 16) & 0x1f;
    int rd = (if_id_out.instruction >> 11) & 0x1f;
    int shamt = (if_id_out.instruction >> 6) & 0x1f;
    int funct = if_id_out.instruction & 0x3f;
    uint32_t imm = (if_id_out.instruction & 0xffff);
    int addr = if_id_out.instruction & 0x3ffffff;
    // Variables to read data into
    uint32_t read_data_1 = 0;
    uint32_t read_data_2 = 0;
    // Read from reg file
    regfile.access(rs, rt, read_data_1, read_data_2, 0, 0, 0);
    id_ex_in.pc = if_id_out.pc;
    id_ex_in.read_data_1 = read_data_1;
    id_ex_in.read_data_2 = read_data_2;
    id_ex_in.imm = imm;
    id_ex_in.rt = rt;
    id_ex_in.rd = rd;
    id_ex_in.control_WB = control.reg_write; //determined from single cycle processor diagram
    id_ex_in.control_M = control.mem_write;
    id_ex_in.control_EX = control.ALU_src; //WARNING: I am not confident about these values. where should the control sigs come from
    //id_ex_in.control_EX is connected to control.ALU_src, ALU_op and RegDst
    return NULL;
}



void* execute_stage(void* arg) {
    // Execution 
    alu.generate_control_inputs(control.ALU_op, funct, opcode);
   
    // Sign Extend Or Zero Extend the immediate
    // Using Arithmetic right shift in order to replicate 1 
    imm = control.zero_extend ? id_ex_out.imm : (id_ex_out.imm >> 15) ? 0xffff0000 | id_ex_out.imm : id_ex_out.imm;
    
    // Find operands for the ALU Execution
    // Operand 1 is always R[rs] -> read_data_1, except sll and srl
    // Operand 2 is immediate if ALU_src = 1, for I-type
    uint32_t operand_1 = id_ex_out.read_data_1;
    uint32_t operand_2 = id_ex_out.control_EX ? imm : id_ex_out.read_data_2;
    uint32_t alu_zero = 0;

    uint32_t alu_result = alu.execute(operand_1, operand_2, alu_zero);
    
    
    uint32_t read_data_mem = 0;
    uint32_t write_data_mem = 0;


    ex_mem_in.branch_target = id_ex_out.pc + (id_ex_in.imm << 2) //NOTE: accurate???
    ex_mem_in.alu_zero = alu_zero;
    ex_mem_in.alu_out = alu_result;
    ex_mem_in.write_data_mem = id_ex_out.read_data_2;
    ex_mem_in.write_reg = id_ex_out.control_EX? id_ex_out.rd : id_ex_out.rt;//from line 105
    return NULL;
}

void* memory_stage(void* arg) {
    return NULL;
}

void* writeback_stage(void* arg) {
    return NULL;
}