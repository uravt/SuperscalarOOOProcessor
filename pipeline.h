#include "processor.h"

class Pipeline {
    private:
        Processor *processor;

    public:
        void fetch();
        void decode();
        void execute();
        void memory();
        void writeback();
};
