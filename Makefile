
CXX = g++
CXXFLAGS= -g -Wall -std=c++11 #-DENABLE_DEBUG
OPTFLAGS= -O3

EXE_NAME=processor
SRCS := main.cpp memory.cpp memory_ooo.cpp processor.cpp processorOOO.cpp instruction_queue.cpp reorder_buffer.cpp load_store_queue.cpp
OBJS := $(SRCS:.cpp=.o)

ZIP_NAME = submission.zip
ZIP_FILES = main.cpp memory.cpp memory_ooo.cpp processor.cpp processorOOO.cpp instruction_queue.cpp reorder_buffer.cpp load_store_queue.cpp regfile.h ALU.h control.h processor.h processorOOO.h memory.h memory_ooo.h reorder_buffer.h prf.h instruction_queue.h functional_units.h load_store_queue.h config.h Makefile

.PHONY: all clean zip debug

all: $(EXE_NAME)

$(EXE_NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

processor.o: processor.cpp regfile.h ALU.h control.h processor.h processorOOO.h
processorOOO.o: processorOOO.cpp processorOOO.h regfile.h ALU.h control.h memory.h memory_ooo.h reorder_buffer.h prf.h instruction_queue.h load_store_queue.h functional_units.h config.h
reorder_buffer.o: reorder_buffer.cpp reorder_buffer.h load_store_queue.h prf.h config.h
instruction_queue.o: instruction_queue.cpp instruction_queue.h load_store_queue.h control.h config.h
load_store_queue.o: load_store_queue.cpp load_store_queue.h prf.h config.h
memory.o: memory.cpp memory.h
memory_ooo.o: memory_ooo.cpp memory_ooo.h memory.h config.h
main.o: main.cpp memory.h processor.h processorOOO.h

debug: $(EXE_NAME)
	gdb $(EXE_NAME)

zip:
	zip $(ZIP_NAME) $(ZIP_FILES)

clean:
	$(RM) $(EXE_NAME) $(OBJS)


