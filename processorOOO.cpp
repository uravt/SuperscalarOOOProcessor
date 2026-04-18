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
}

ProcessorOOO::IF_ID ProcessorOOO::fetch_stage() //IO
{
    ProcessorOOO::IF_ID if_id;
    memset(&if_id, 0, sizeof(if_id));

    //Check if instruction queue is full


    uint32_t inst;
    //Cache hit/miss needs to go though nbc
    memory->access(regfile.pc, inst, 0, 1, 0);

    if_id.instruction = inst;
    if_id.pc = regfile.pc;

    regfile.pc += 4;

    return if_id;

}
ProcessorOOO::ID_RN ProcessorOOO::decode_stage(ProcessorOOO::IF_ID if_id) //IO
{
    ProcessorOOO::ID_RN id_rn;
    memset(&id_rn, 0, sizeof(id_rn));

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
    id_rn.addr = inst & 0x3ffffff;
    return id_rn;
}

//Rename Issue Combined
ProcessorOOO::RN_DP ProcessorOOO::rename_stage(ProcessorOOO::ID_RN id_rn) //IO
{
    ProcessorOOO::RN_DP rn_dp;
    memset(&rn_dp, 0, sizeof(rn_dp));

    //add instruction to ROB
    ROBEntry robEntry;
    memset(&robEntry, 0, sizeof(robEntry));
    robEntry.completed = false;
    int dest_arch_reg = id_rn.control.reg_dest ? id_rn.rd : id_rn.rt;
    uint32_t read_data_1 = 0;
    uint32_t read_data_2 = 0;
    prf.access(id_rn.rs,id_rn.rt,read_data_1,read_data_2,0,0,0); // Allocate physical reg space
    rob.insert(dest_arch_reg, prf.get_mapping(dest_arch_reg), id_rn.control.reg_write);

    uint32_t operand_1 = id_rn.control.shift ? id_rn.shamt : id_rn.read_data_1;
    uint32_t operand_2 = id_rn.control.ALU_src ? id_rn.imm : id_rn.read_data_2;
    

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
    iq.add(decoded_instruction);

    //rn_dp might just not need to pass any info. NEED TO verify
    return rn_dp;
    
}
ProcessorOOO::DP_EX ProcessorOOO::dispatch_stage(ProcessorOOO::RN_DP rn_dp) //OOO
{
    ProcessorOOO::DP_EX dp_ex;
    memset(&dp_ex, 0, sizeof(dp_ex));

    //loop though issue queue
    //send a ready instruction
    return dp_ex;
}
ProcessorOOO::EX_WB ProcessorOOO::execute_stage(ProcessorOOO::DP_EX dp_ex) //OOO
{
    ProcessorOOO::EX_WB ex_wb;
    memset(&ex_wb, 0, sizeof(ex_wb));
    
    //execute
    uint32_t op1 = 0, op2 = 0; // TODO: pull from pipeline register
    uint32_t result = alu.execute(op1, op2, *(new uint32_t));
    //wakeup instruction
    return ex_wb;
}
ProcessorOOO::WB_CM ProcessorOOO::writeback_stage(ProcessorOOO::EX_WB ex_wb) //OOO
{
    ProcessorOOO::WB_CM wb_cm;
    memset(&wb_cm, 0, sizeof(wb_cm));

    control_t sig = ex_wb.control;
    //add results to commit buffer
    if(ex_wb.phys_rd != 0) { //we don't have to do writeback for the zero register
        prf.write(ex_wb.phys_rd, ex_wb.result);

        
    }

    return wb_cm;
}
void ProcessorOOO::commit_stage(ProcessorOOO::WB_CM wb_cm) //IO
{
    int num_commited = 0;
    while(num_commited < config::PIPELINE_WIDTH) {
        if(rob.commit(prf)) {
            num_commited++;
        } else {
            break; //as soon as we can't commit we should break
        }
    }
}



