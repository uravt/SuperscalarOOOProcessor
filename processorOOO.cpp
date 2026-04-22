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
    i_nbc.initialize();
    d_nbc.initialize();
    i_nbc.memory = memory;
    d_nbc.memory = memory;
}

void ProcessorOOO::out_of_order_advance() {
    initialize(2);

    i_nbc.checkReady();
    d_nbc.checkReady();

    for (auto &r : d_nbc.readyResponses) //data cache found ready instructions. wake up components
    {
        prf.write(r.reg, r.data);
        iq.broadcast_ready(r.reg);
        rob.set_ready(r.rob_index);
    }
    d_nbc.readyResponses.clear();
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
        for(int j = 0; j < config::PIPELINE_WIDTH; j++) {
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

        uint64_t seq = next_seq++;

        //add instruction to ROB
        ROBEntry robEntry{};
        int rob_index = rob.insert(seq, dest_arch_reg, new_phys_reg, old_phys_reg);

        //add instruction to IQ
        iq_instr instr{};
        instr.seq = seq;
        instr.pc = id_rn.pc;
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
        instr.control = id_rn.control;
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
    for(int i = 0; i < config::PIPELINE_WIDTH; i++) {
        FunctionalUnit &unit = fu.get(i);
        if(!unit.ready) { //we have something to execute
            iq_instr instr = unit.instr;

            unit.alu.generate_control_inputs(instr.control.ALU_op, instr.funct, instr.opcode);
            uint32_t imm = instr.control.zero_extend ? instr.imm : (instr.imm >> 15) ? 0xffff0000 | instr.imm : instr.imm;
            uint32_t operand_1 = instr.control.shift ? instr.shamt : prf.read(instr.rs);
            uint32_t operand_2 = instr.control.ALU_src ? imm : prf.read(instr.rt);
            uint32_t alu_zero = 0;



            uint32_t alu_result = unit.alu.execute(operand_1, operand_2, alu_zero);

            if(instr.control.mem_read)//We have a load. stop from going to writeback stage
            {
                bool success = d_nbc.allocateMSHR(alu_result, instr.rob_index, instr.rd);
                if (success)// MSHR is allocated successfully
                {
                    unit.ready = true;
                    unit.has_result = false;
                }
                continue;
            }

            bool branch_taken = instr.control.branch && 
            ((instr.control.bne && !alu_zero) || 
            (!instr.control.bne && alu_zero));

            bool jump = instr.control.jump || instr.control.jump_reg;

            if(branch_taken || jump) {
                uint32_t target;
                if(branch_taken) {
                    target = instr.pc + (imm << 2);
                }
                else if(instr.control.jump) {
                    target = (instr.pc & 0xf0000000) | (instr.addr << 2);
                }
                else {
                    target = prf.read(instr.rs);
                }

                // Track oldest mispredicted branch across all FUs this cycle
                if(!squash_pending || instr.seq < squash_seq) {
                    squash_pending = true;
                    squash_seq = instr.seq;
                    squash_target_pc = target;
                }
            }

            unit.result = alu_result;
            unit.ready = true;//Does has ready need to be set true here?
        }

    }

    if(squash_pending) {
        perform_squash();
    }
    
   
    // Sign Extend Or Zero Extend the immediate
    // Using Arithmetic right shift in order to replicate 1 
    
    
    // Find operands for the ALU Execution
    // Operand 1 is always R[rs] -> read_data_1, except sll and srl
    // Operand 2 is immediate if ALU_src = 1, for I-type


    

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
void ProcessorOOO::writeback_stage() //OOO
{
    for(int i = 0; i < config::NUM_ALUS; i++) { //this only works because NUM_ALUS == PIPELINE_WIDTH, maybe make this more robust
        FunctionalUnit unit = fu.get(i);
        if(unit.instr.control.mem_read) continue;
        if(unit.instr.control.reg_write && unit.has_result && unit.instr.rd != 0) { //we don't have to do prf writeback for the zero register
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

void ProcessorOOO::perform_squash() {
    iq.squash(squash_seq);
    fu.squash(squash_seq);
    rob.squash(squash_seq, prf);

    for(int i = 0; i < config::PIPELINE_WIDTH; i++) {
        if_id_buffer[i].in_use = false;
        id_rn_buffer[i].in_use = false;
    }

    regfile.pc = squash_target_pc;
    squash_pending = false;
    squash_seq = 0;
    squash_target_pc = 0;
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


