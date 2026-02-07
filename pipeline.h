#include "processor.h"

class Pipeline {
    private:
        Processor *processor;

    public:
        Pipeline(Processor *processor) {this->processor = processor;};
        void fetch();
        void decode();
        void execute();
        void memory();
        void writeback();
};
