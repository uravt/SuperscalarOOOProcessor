#include "memory.h"
#include "regfile.h"
#include "ALU.h"
#include "control.h"
#include <sstream>
class ProcessorOOO {
    private:
        int opt_level;
        ALU alu;
        control_t control;
        Memory *memory;
        Registers regfile;
        // add other structures as needed
        //pipeline registers
        //values got based on diagram registers in and out
        //data types based on single processor implementation

    struct IF_ID
    {
        uint32_t pc;
        uint32_t instruction;

        std::string toString() const
        {
            std::ostringstream oss;
            oss << "pc:" << std::hex << pc
                << "|instruction:" << std::hex << instruction;
            return oss.str();
        }
    };
    struct ID_RN
    {

    };
    struct RN_IS
    {

    };
    struct IS_DP
    {

    };
    struct DP_EX
    {

    };
    struct EX_WB
    {

    };
    struct WB_CM
    {

    };
    IF_ID if_id;
    
 
    public:
        Processor(Memory *mem)
        {
            regfile.pc = 0;
            memory = mem;

            memset(&if_id_in, 0, sizeof(if_id_in));
            memset(&if_id_out, 0, sizeof(if_id_out));
            memset(&id_ex_in, 0, sizeof(id_ex_in));
            memset(&id_ex_out, 0, sizeof(id_ex_out));
            memset(&ex_mem_in, 0, sizeof(ex_mem_in));
            memset(&ex_mem_out, 0, sizeof(ex_mem_out));
            memset(&mem_wb_in, 0, sizeof(mem_wb_in));
            memset(&mem_wb_out, 0, sizeof(mem_wb_out));
        }

        // Get PC
        uint32_t getPC() { return opt_level == 0 ? regfile.pc : pc_history[0]; }

        // Prints the Register File
        void printRegFile() { regfile.print(); }
        
        // Initializes the processor appropriately based on the optimization level
        void initialize(int opt_level);

        // Advances the processor to an appropriate state every cycle
        void advance(); 
};
