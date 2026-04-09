#include <iostream>
#include <queue> 
struct iq_instr
{
    int opcode;
    int rs;
    int rt;
    int rd;
    int shamt;
    int funct;
    uint32_t imm;
    int addr;

    bool ready;
}
class Instruction_Queue
{
    private:
        queue<iq_instr> iq;
    public:
        void push(iq_instr instr);
        void pop();

}