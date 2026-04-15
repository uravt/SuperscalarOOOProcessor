
CXX = g++
CXXFLAGS= -g -Wall -std=c++11 #-DENABLE_DEBUG
OPTFLAGS= -O3

EXE_NAME=processor
SRCS := main.cpp memory.cpp processor.cpp processorOOO.cpp instruction_queue.cpp
OBJS := $(SRCS:.cpp=.o)

ZIP_NAME = submission.zip
ZIP_FILES = main.cpp memory.cpp processor.cpp processorOOO.cpp instruction_queue.cpp regfile.h ALU.h control.h processor.h processorOOO.h memory.h reorder_buffer.h prf.h instruction_queue.h config.h Makefile

.PHONY: all clean zip

all: $(EXE_NAME)

$(EXE_NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

processor.o: regfile.h ALU.h control.h processor.h processorOOO.h
processorOOO.o: processorOOO.h regfile.h ALU.h control.h memory.h reorder_buffer.h prf.h instruction_queue.h config.h
instruction_queue.o: instruction_queue.h config.h
memory.o: memory.h
main.o: memory.h processor.h

zip:
	zip $(ZIP_NAME) $(ZIP_FILES)

clean:
	$(RM) $(EXE_NAME) $(OBJS)


