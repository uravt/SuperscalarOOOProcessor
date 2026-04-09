#include "instruction_queue.h"
class Instruction_Queue
{
    public:
        void push(iq_instr instr)
        {
            iq.push(instr);
            instr.ready = true;
        }
        void pop()
        {
            iq.pop();
        }

}