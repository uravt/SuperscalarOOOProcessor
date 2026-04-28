#include <cstdint>
#include <cstring>
#include <cstdio>
#include <iostream>
#include "processorOOO.h"


using namespace std;

#define ENABLE_DEBUG
#ifdef ENABLE_DEBUG
#define DEBUG(x) x
#else
#define DEBUG(x)
#endif

void ProcessorOOO::initialize(int level)
{
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
    i_nbc.memory = memory;
    d_nbc.memory = memory;
    i_nbc.initialize(memory->getL1MissPenalty());
    d_nbc.initialize(memory->getL1MissPenalty());
}

void ProcessorOOO::out_of_order_advance()
{
    // prf.print_debug();
    rob.print();
    iq.print();
    lsq.print();
    fu.print();

    // advance code
    commit_stage();
    writeback_stage();
    memory_stage();
    execute_stage();
    dispatch_stage();
    rename_stage();
    decode_stage();
    fetch_stage();
    // DEBUG(fprintf(stderr, "[advance] pc=%u if_id.in_use=%d id_rn.in_use=%d rob.full=%d iq.full=%d\n",
    //     regfile.pc, if_id_buffer[0].in_use, id_rn_buffer[0].in_use, rob.full(), iq.full());)
}

void ProcessorOOO::fetch_stage() // IO
{
    for (int i = 0; i < config::PIPELINE_WIDTH; i++)
    {
        IF_ID &reg = if_id_buffer[i];
        if (reg.in_use)
        {
            continue;
        }

        if (prf.pc >= end_pc)
        {
            break; // past end of program
        }

        uint32_t inst = 0;
        if (!memory->access(prf.pc, inst, 0, 1, 0))
        {
            break; // cache miss, stall fetch this cycle
        }

        reg.instruction = inst;
        reg.pc = prf.pc;
        reg.in_use = true;

        bool taken = bp.predictBranch(reg.pc);
        BTBResult lookup = bp.get_target(prf.pc);

        if(taken && lookup.hit) {
            prf.pc = lookup.targetPC;
            reg.predicted_taken = true;
            reg.predicted_target = prf.pc;
        } else {
            prf.pc += 4;
            reg.predicted_taken = false;
            reg.predicted_target = prf.pc;
        }
        
    }
}
void ProcessorOOO::decode_stage() // IO
{
    // Compact id_rn so any leftover (unrenamed) entries sit at low indices in
    // program order. Without this, a partial rename stall leaves holes that
    // decode refills with newer instructions, breaking program order at rename.
    int w = 0;
    for (int r = 0; r < config::PIPELINE_WIDTH; r++) {
        if (id_rn_buffer[r].in_use) {
            if (w != r) {
                id_rn_buffer[w] = id_rn_buffer[r];
                id_rn_buffer[r].in_use = false;
            }
            w++;
        }
    }

    // Same for if_id: a partial decode stall must not let fetch interleave
    // newer instructions ahead of older ones in slot order.
    int wf = 0;
    for (int r = 0; r < config::PIPELINE_WIDTH; r++) {
        if (if_id_buffer[r].in_use) {
            if (wf != r) {
                if_id_buffer[wf] = if_id_buffer[r];
                if_id_buffer[r].in_use = false;
            }
            wf++;
        }
    }

    for (int i = 0; i < config::PIPELINE_WIDTH; i++)
    {
        ID_RN &reg = id_rn_buffer[i];
        if (reg.in_use)
        {
            continue;
        }

        int src = -1;
        for (int j = 0; j < config::PIPELINE_WIDTH; j++)
        {
            if (if_id_buffer[j].in_use)
            {
                src = j;
                break;
            }
        }

        if (src == -1)
        {
            break;
        }

        IF_ID &to_decode = if_id_buffer[src];
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
        update_reg_src_usage(reg.opcode, reg.funct, reg);
        reg.in_use = true;
        reg.predicted_taken = to_decode.predicted_taken;
        reg.predicted_target = to_decode.predicted_target;

        to_decode.in_use = false;
    }
}

// Rename Issue Combined
void ProcessorOOO::rename_stage() // IO
{
    for (int i = 0; i < config::PIPELINE_WIDTH; i++)
    {
        ID_RN &id_rn = id_rn_buffer[i];
        std::cout << ProcessorOOO::toString(id_rn) << "\n";
        if (!id_rn.in_use)
            continue;

        if (rob.full() || iq.full() ||
            (id_rn.control.mem_read  && lsq.lq_full()) ||
            (id_rn.control.mem_write && lsq.sq_full())) {
            break; //we want to stall, potentially more code needed
        }

        int dest_arch_reg = id_rn.control.reg_dest ? id_rn.rd : id_rn.rt;
        bool writes_reg = id_rn.control.reg_write && dest_arch_reg != 0;

        if (writes_reg && !prf.has_free_phys_reg())
            break;

        int rs_phys_reg = id_rn.reads_rs ? prf.get_mapping(id_rn.rs) : 0;
        int rt_phys_reg = id_rn.reads_rt ? prf.get_mapping(id_rn.rt) : 0;

        bool rs_ready = !id_rn.reads_rs || prf.ready(rs_phys_reg) || id_rn.rs == 0;
        bool rt_ready = !id_rn.reads_rt || prf.ready(rt_phys_reg) || id_rn.rt == 0;

        int old_phys_reg = 0, new_phys_reg = 0;
        if (writes_reg)
        {
            old_phys_reg = prf.get_mapping(dest_arch_reg);
            new_phys_reg = prf.assign_mapping(dest_arch_reg);
        }

        uint64_t seq = next_seq++;
        //add to lsq
        int load_index = -1;
        int store_index = -1;
        if(id_rn.control.mem_read) { //load
            LoadEntry entry{};
            entry.seq = seq;
            entry.pc = id_rn.pc;
            entry.dest_phys_reg = new_phys_reg;
            entry.byte = id_rn.control.byte;
            entry.halfword = id_rn.control.halfword;
            load_index = lsq.add_load(entry);
        } else if(id_rn.control.mem_write) { //store
            StoreEntry entry{};
            entry.src = rt_phys_reg;
            entry.src_ready = rt_ready;
            entry.seq = seq;
            entry.pc = id_rn.pc;
            entry.byte = id_rn.control.byte;
            entry.halfword = id_rn.control.halfword;
            store_index = lsq.add_store(entry);
        }

        // add instruction to ROB
        int rob_index = rob.insert(seq, dest_arch_reg, new_phys_reg, old_phys_reg, load_index, store_index);

        // backfill rob_index in the LSQ entry for display/debug
        if(load_index != -1)  lsq.get_load(load_index).rob_index = rob_index;
        if(store_index != -1) lsq.get_store(store_index).rob_index = rob_index;

        // add instruction to IQ
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
        instr.load_index = load_index;
        instr.store_index = store_index;
        instr.predicted_taken = id_rn.predicted_taken;
        iq.add(instr);

        id_rn.in_use = false;
    }
}
void ProcessorOOO::dispatch_stage() // OOO
{
    // loop though issue queue
    int num_dispatched = 0;
    while (num_dispatched < config::PIPELINE_WIDTH && !fu.full())
    {
        // get a valid instr from iq, remove from iq, send it to a functional unit
        int oldest_ready = iq.get_oldest_ready(lsq);

        if (oldest_ready == -1)
        {
            break;
        }

        iq_instr instr = iq.get(oldest_ready);
        iq.remove(oldest_ready);
        fu.issue_to_unit(instr);
    }
}
void ProcessorOOO::execute_stage() // OOO
{
    for (int i = 0; i < config::NUM_ALUS; i++)
    {
        FunctionalUnit &unit = fu.get(i);
        if (!unit.ready)
        { // we have something to execute
            iq_instr instr = unit.instr;

            unit.alu.generate_control_inputs(instr.control.ALU_op, instr.funct, instr.opcode);
            uint32_t imm = instr.control.zero_extend ? instr.imm : (instr.imm >> 15) ? 0xffff0000 | instr.imm
                                                                                     : instr.imm;
            uint32_t operand_1 = instr.control.shift ? instr.shamt : prf.read(instr.rs);
            uint32_t operand_2 = instr.control.ALU_src ? imm : prf.read(instr.rt);
            uint32_t alu_zero = 0;

            uint32_t alu_result = unit.alu.execute(operand_1, operand_2, alu_zero);

            uint32_t load_result = 0;
            if(instr.control.mem_read) { //loads
                LoadEntry &entry = lsq.get_load(instr.load_index);
                entry.addr = alu_result;
                entry.addr_ready = true;

                // If an older store to this same address has its addr but not
                // its data yet, we can't safely forward (try_forward would skip
                // it and fall through to stale L1). Stall the load in the FU
                // until the store's data is broadcast.
                if(lsq.has_pending_forward(entry.seq, entry.addr)) {
                    continue;
                }

                uint32_t val = 0;
                if(lsq.try_forward(entry.seq, val, prf)) { //store to load forwarding
                    load_result = val;
                    lsq.mark_load_complete(entry.seq);
                }
                else if(memory->probeL1(entry.addr, val)) { //L1 hit
                    load_result = val;
                    lsq.mark_load_complete(entry.seq);
                } else { //L1 miss — route through NBC, which models full miss latency
                         //and walks the hierarchy when the countdown elapses
                    bool success = d_nbc.allocateMSHR(entry.addr, instr.rob_index, instr.rd, instr.seq);
                    if(success) {
                        unit.ready = true;
                        unit.has_result = false;
                    }
                    continue;
                }
            } else if(instr.control.mem_write) { //stores
                StoreEntry &entry = lsq.get_store(instr.store_index);
                entry.addr = alu_result;
                entry.addr_ready = true;
                entry.data = prf.read(entry.src);
            }

            bool branch_taken = instr.control.branch &&
                                ((instr.control.bne && !alu_zero) ||
                                 (!instr.control.bne && alu_zero));

            if(instr.control.branch) {
                bp.updatePredictor(instr.pc, branch_taken);
            }

            bool jump = instr.control.jump || instr.control.jump_reg;
            // Default to fall-through so a predicted-taken-but-not-taken branch
            // squashes to instr.pc + 4 instead of uninitialized memory.
            uint32_t target = instr.pc + 4;
            if (branch_taken || jump)
            {
                if (branch_taken)
                {
                    target = instr.pc + 4 + (imm << 2);
                    bp.update_btb(instr.pc, target, branch_taken);
                }
                else if (instr.control.jump)
                {
                    target = ((instr.pc + 4) & 0xf0000000) | (instr.addr << 2);
                    bp.update_btb(instr.pc, target, branch_taken);
                }
                else
                {
                    target = prf.read(instr.rs);
                }
            }


            bool mispredicted = false;

            if (instr.control.branch) {
                // Direction was wrong, OR we jumped to the wrong place
                if (instr.predicted_taken != branch_taken || (branch_taken && instr.predicted_target != target)) {
                    mispredicted = true;
                }
            } else if (jump) {
                // Jumps are always taken, but BTB might have missed or given wrong address (e.g. jr $ra)
                if (instr.predicted_target != target) {
                    mispredicted = true;
                }
            }

            if (mispredicted) {
                if (!squash_pending || instr.seq < squash_seq) {
                    squash_pending = true;
                    squash_seq = instr.seq;
                    squash_target_pc = target; // or PC+4 if we wrongly predicted a jump
                }
            }


            unit.result = !instr.control.mem_read ? alu_result : load_result;
            unit.result_instr = instr;
            unit.has_result = true;
            unit.ready = true;
        }
    }

    if (squash_pending)
    {
        perform_squash();
    }
}
void ProcessorOOO::memory_stage() {
    i_nbc.checkReady();
    d_nbc.checkReady();

    for (auto &r : d_nbc.readyResponses) //data cache found ready instructions. wake up components
    {
        prf.write(r.reg, r.data);
        iq.broadcast_ready(r.reg);
        lsq.broadcast_ready(r.reg);
        rob.set_ready(r.rob_index);
    }
    d_nbc.readyResponses.clear();

    //committed stores which can now be written to the cache should be evicted from the sq
    //(usually already drained in commit_stage; this picks up any stalled by mem backpressure)
    auto er = lsq.evict_commited_stores(memory, prf, end_pc);
    if (er.smc_detected &&
        (!squash_pending || er.smc_seq < squash_seq)) {
        squash_pending = true;
        squash_seq = er.smc_seq;
        squash_target_pc = er.smc_pc + 4;
    }

}
void ProcessorOOO::writeback_stage() // OOO
{
    for (int i = 0; i < config::NUM_ALUS; i++)
    {
        FunctionalUnit &unit = fu.get(i);
        if (!unit.has_result)
            continue;

        if (unit.result_instr.control.reg_write && unit.result_instr.rd != 0)
        {
            prf.write(unit.result_instr.rd, unit.result);
            iq.broadcast_ready(unit.result_instr.rd);
            lsq.broadcast_ready(unit.result_instr.rd);
        }
        rob.set_ready(unit.result_instr.rob_index);

        unit.has_result = false;
    }
}
void ProcessorOOO::commit_stage() // IO
{
    int num_commited = 0;
    while (num_commited < config::PIPELINE_WIDTH)
    {
        if (!rob.commit(prf, lsq))
        {
            break; // as soon as we can't commit we should break
        }
        num_commited++;

        // Drain just-committed stores to memory now, so that if a store wrote
        // into the instruction-memory range (SMC), we can squash everything
        // that follows it before any of those entries commit this same cycle.
        auto er = lsq.evict_commited_stores(memory, prf, end_pc);
        if (er.smc_detected &&
            (!squash_pending || er.smc_seq < squash_seq))
        {
            squash_pending = true;
            squash_seq = er.smc_seq;
            squash_target_pc = er.smc_pc + 4;
            break;
        }
    }
}


void ProcessorOOO::perform_squash()
{
    iq.squash(squash_seq);
    fu.squash(squash_seq);
    rob.squash(squash_seq, prf);
    lsq.squash(squash_seq);
    d_nbc.squash(squash_seq);

    for (int i = 0; i < config::PIPELINE_WIDTH; i++)
    {
        if_id_buffer[i].in_use = false;
        id_rn_buffer[i].in_use = false;
    }

    prf.pc = squash_target_pc;
    squash_pending = false;
    squash_seq = 0;
    squash_target_pc = 0;
}

void ProcessorOOO::update_reg_src_usage(int opcode, int funct, ProcessorOOO::ID_RN &reg)
{
    switch (opcode)
    {
    case 0x00:                                                            // R-type
        reg.reads_rs = (funct != 0x00 && funct != 0x02 && funct != 0x03); // not sll/srl/sra
        reg.reads_rt = (funct != 0x08 && funct != 0x09);                  // not jr/jalr
        break;
    case 0x02:
    case 0x03: // j, jal
        reg.reads_rs = false;
        reg.reads_rt = false;
        break;
    case 0x0f: // lui
        reg.reads_rs = false;
        reg.reads_rt = false;
        break;
    case 0x04:
    case 0x05: // beq, bne
        reg.reads_rs = true;
        reg.reads_rt = true;
        break;
    case 0x01:
    case 0x06:
    case 0x07: // bltz/bgez, blez, bgtz
        reg.reads_rs = true;
        reg.reads_rt = false;
        break;
    case 0x23:
    case 0x20:
    case 0x21:
    case 0x24:
    case 0x25: // loads
        reg.reads_rs = true;
        reg.reads_rt = false; // rt is destination
        break;
    case 0x2b:
    case 0x28:
    case 0x29: // stores
        reg.reads_rs = true;
        reg.reads_rt = true; // store value
        break;
    default: // I-type ALU (addi, andi, ori, etc.)
        reg.reads_rs = true;
        reg.reads_rt = false; // immediate replaces rt
        break;
    }
}
