#include <cstdint>
#include <cstring>
#include <cstdio>
#include <iostream>
#include "processorOOO.h"
#include "reorder_buffer.h"
#include "instruction_queue.h"


using namespace std;

#define ENABLE_DEBUG
#ifdef ENABLE_DEBUG
#define DEBUG(x) x
#else
#define DEBUG(x) 
#endif

void ProcessorOOO::initialize(int level) {
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

void ProcessorOOO::out_of_order_advance() {
    initialize(2);
    //advance code
    commit_stage();
    writeback_stage();
    execute_stage();
    dispatch_stage();
    issue_stage();
    rename_stage();
    decode_stage();
    fetch_stage();
}

void ProcessorOOO::fetch_stage() //IO
{
    //Check if instruction queue is full


    uint32_t inst;
    //Cache hit/miss needs to go though nbc
    memory->access(regfile.pc, inst, 0, 1, 0);

    if_id.instruction = inst;
    if_id.pc = regfile.pc;

    regfile.pc += 4;

    //add to instruction queue
}
void ProcessorOOO::decode_stage() //IO
{
    uint32_t inst = if_id.instruction;

    control.decode(inst);

    id_rn.rs = (inst >> 21) & 0x1f;
    id_rn.rt = (inst >> 16) & 0x1f;
    id_rn.rd = (inst >> 11) & 0x1f;
    id_rn.imm = inst & 0xffff;
    id_rn.shamt = (inst >> 6) & 0x1f;
    id_rn.opcode = (inst >> 26) & 0x3f;
    id_rn.funct = inst & 0x3f;
    id_rn.pc = if_id.pc;
    id_rn.control = control;
}
void ProcessorOOO::rename_stage() //IO
{
    //this all needs to be redone tbh but i just want it to compile for now
    //add instruction to ROB
    ROBEntry robEntry;
    memset(&robEntry, 0, sizeof(robEntry));
    robEntry.completed = false;
<<<<<<< Updated upstream
    robEntry.dest_arch_reg = id_rn.control.reg_dest ? id_rn.rd : id_rn.rt;
    robEntry.dest_phys_reg = prf.assign_mapping(robEntry.dest_arch_reg); // Allocate physical reg space
    rob.insert(robEntry.dest_arch_reg, robEntry.dest_phys_reg, prf.get_mapping(robEntry.dest_arch_reg)); // Insert into ROB with old_phys_reg as -1 for now

    uint32_t operand_1 = id_rn.control.shift ? 0 : 0; // TODO: shamt / read_data_1
    uint32_t operand_2 = id_rn.control.ALU_src ? id_rn.imm : 0; // TODO: read_data_2
=======
    int dest_arch_reg = id_rn.control.reg_dest ? id_rn.rd : id_rn.rt;
    uint32_t read_data_1 = 0;
    uint32_t read_data_2 = 0;
    prf.access(id_rn.rs,id_rn.rt,read_data_1,read_data_2,0,0,0); // Allocate physical reg space
    rob.insert(dest_arch_reg, prf.get_mapping(dest_arch_reg), id_rn.control.reg_write);

    uint32_t operand_1 = id_rn.control.shift ? id_rn.shamt : id_rn.read_data_1;
    uint32_t operand_2 = id_rn.control.ALU_src ? id_rn.imm : id_rn.read_data_2;
>>>>>>> Stashed changes
    

    //preform register renaming
    if(id_rn.control.reg_write)
    {
        prf.assign_mapping(id_rn.control.reg_dest ? id_rn.rd : id_rn.rt);
    }
    prf.assign_mapping(id_rn.rs);

    if(!id_rn.control.ALU_src)
    {
        prf.assign_mapping(id_rn.rt);
    }

    //allocate abd bind ROB and IQ
    iq_instr decoded_instruction;
    memset(&decoded_instruction, 0, sizeof(decoded_instruction));
    decoded_instruction.opcode = id_rn.opcode;
    decoded_instruction.rs = prf.get_mapping(id_rn.rs);
    decoded_instruction.rt = prf.get_mapping(id_rn.rt);
    decoded_instruction.rd = prf.get_mapping(id_rn.rd);
    decoded_instruction.shamt = id_rn.shamt;
    decoded_instruction.funct = id_rn.funct;
    decoded_instruction.imm = id_rn.imm;
    decoded_instruction.addr = id_rn.addr;
    iq.push(decoded_instruction);
    decoded_instruction.ready = true;
}
void ProcessorOOO::issue_stage() //IO
{
    //add to issue queue
}
void ProcessorOOO::dispatch_stage() //OOO
{
    //loop though issue queue
    //send a ready instruction
}
void ProcessorOOO::execute_stage() //OOO
{
    //execute
    uint32_t op1 = 0, op2 = 0; // TODO: pull from pipeline register
    uint32_t result = alu.execute(op1, op2, *(new uint32_t));
    //wakeup instruction
}
void ProcessorOOO::writeback_stage() //OOO
{
    //add results to commit buffer
}
void ProcessorOOO::commit_stage() //IO
{
    //commit results to reg file in order according to ROB
}



