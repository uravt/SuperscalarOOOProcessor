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
    rename_stage();
    decode_stage();
    fetch_stage();
}

void ProcessorOOO::fetch_stage() //IO
{
    for(int i = 0; i < config::PIPELINE_WIDTH; i++) {
        IF_ID &reg = if_id_buffer[i];
        if(reg.in_use) {
            continue;
        }

        uint32_t inst;
        //Cache hit/miss needs to go though nbc
        memory->access(regfile.pc, inst, 0, 1, 0);

        reg.instruction = inst;
        reg.pc = regfile.pc;
        reg.in_use = true;

        regfile.pc += 4;
    }
}
void ProcessorOOO::decode_stage() //IO
{
    for(int i = 0; i < config::PIPELINE_WIDTH; i++) {
        ID_RN &reg = id_rn_buffer[i];
        if(reg.in_use) {
            continue;
        }

        IF_ID to_decode;
        for(int j = 0; j < config::PIPELINE_WIDTH; i++) {
            if(if_id_buffer[j].in_use) {
                to_decode = if_id_buffer[j];
            }
        }

        if(!to_decode.in_use) {//fetch was not able to send any new instrs, or there were no more instrs
            break;
        } 

        uint32_t inst = to_decode.instruction;

        control.decode(inst);
        reg.rs = (inst >> 21) & 0x1f;
        reg.rt = (inst >> 16) & 0x1f;
        reg.rd = (inst >> 11) & 0x1f;
        reg.imm = inst & 0xffff;
        reg.shamt = (inst >> 6) & 0x1f;
        reg.opcode = (inst >> 26) & 0x3f;
        reg.funct = inst & 0x3f;
        reg.pc = to_decode.pc;
        reg.control = control;
        reg.addr = inst & 0x3ffffff;
    }

    
}

//Rename Issue Combined
void ProcessorOOO::rename_stage() //IO
{
    for(int i = 0; i < config::PIPELINE_WIDTH; i++) {
        if(rob.full() || iq.full()) {
            break; //we want to stall, probably need to add more code for that here
        }

        ID_RN id_rn = id_rn_buffer[i];

        int dest_arch_reg = id_rn.control.reg_dest ? id_rn.rd : id_rn.rt;
        bool writes_reg = id_rn.control.reg_write && dest_arch_reg != 0;

        if (writes_reg && !prf.has_free_phys_reg()) break;

        int rs_phys_reg = id_rn.reads_rs ? prf.get_mapping(id_rn.rs) : 0;
        int rt_phys_reg = id_rn.reads_rt ? prf.get_mapping(id_rn.rt) : 0;

        bool rs_ready = !id_rn.reads_rs || prf.ready(rs_phys_reg) || id_rn.rs == 0;
        bool rt_ready = !id_rn.reads_rt || prf.ready(rt_phys_reg) || id_rn.rt == 0;


        int old_phys_reg = 0, new_phys_reg = 0;
        if(writes_reg) {
            old_phys_reg = prf.get_mapping(dest_arch_reg);
            new_phys_reg = prf.assign_mapping(dest_arch_reg);
        }

        //add instruction to ROB
        ROBEntry robEntry{};
        int rob_index = rob.insert(dest_arch_reg, new_phys_reg, old_phys_reg);

        //add instruction to IQ
        iq_instr instr{};
        instr.opcode = id_rn.opcode;
        instr.rs = rs_phys_reg;
        instr.rt = rt_phys_reg;
        instr.rd = new_phys_reg;
        instr.shamt = id_rn.shamt;
        instr.funct = id_rn.funct;
        instr.imm = id_rn.imm;
        instr.addr = id_rn.addr;
        instr.rob_index = rob_index;
        instr.rs_ready = rs_ready;
        instr.rt_ready = rt_ready;
        iq.add(instr);
    }
}
void ProcessorOOO::dispatch_stage() //OOO
{
    //loop though issue queue
    int num_dispatched = 0;
    while(num_dispatched < config::PIPELINE_WIDTH && !fu.full()) {
        //get a valid instr from iq, remove from iq, send it to a functional unit
        int oldest_ready = iq.get_oldest_ready();

        if(oldest_ready == -1) {
            break;
        }

        iq_instr instr = iq.get(oldest_ready);
        iq.remove(oldest_ready);
        fu.issue_to_unit(instr);
    }
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
    for(int i = 0; i < config::NUM_ALUS; i++) { //this only works because NUM_ALUS == PIPELINE_WIDTH, maybe make this more robust
        FunctionalUnit unit = fu.get(i);
        if(unit.has_result && unit.instr.rd != 0) { //we don't have to do prf writeback for the zero register
            prf.write(unit.instr.rd, unit.result);
            iq.broadcast_ready(unit.instr.rd);
        }
        rob.set_ready(unit.instr.rob_index);
    }
}
void ProcessorOOO::commit_stage() //IO
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

void ProcessorOOO::update_reg_src_usage(int opcode, int funct, ProcessorOOO::ID_RN &reg) {
    switch (opcode) {
        case 0x00: // R-type
            reg.reads_rs = (funct != 0x00 && funct != 0x02 && funct != 0x03); // not sll/srl/sra
            reg.reads_rt = (funct != 0x08 && funct != 0x09); // not jr/jalr
            break;
        case 0x02: case 0x03: // j, jal
            reg.reads_rs = false;
            reg.reads_rt = false;
            break;
        case 0x0f: // lui
            reg.reads_rs = false;
            reg.reads_rt = false;
            break;
        case 0x04: case 0x05: // beq, bne
            reg.reads_rs = true;
            reg.reads_rt = true;
            break;
        case 0x01: case 0x06: case 0x07: // bltz/bgez, blez, bgtz
            reg.reads_rs = true;
            reg.reads_rt = false;
            break;
        case 0x23: case 0x20: case 0x21: case 0x24: case 0x25: // loads
            reg.reads_rs = true;
            reg.reads_rt = false;  // rt is destination
            break;
        case 0x2b: case 0x28: case 0x29: // stores
            reg.reads_rs = true;
            reg.reads_rt = true;   // store value
            break;
        default: // I-type ALU (addi, andi, ori, etc.)
            reg.reads_rs = true;
            reg.reads_rt = false;  // immediate replaces rt
            break;
    }
}


