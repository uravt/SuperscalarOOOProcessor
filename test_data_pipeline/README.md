## Instructions to use the tests.

You can compile all the tests by using the Makefile. `make clean; make` should clean and 
compile all tests (test.s) files within the directories. 

You can find the binaries in the `./bin` directory and the disassembled files in 
the `./dump` directory. 

## Custom tests

You can create custom test cases by following the same file structure as the 
existing test files and placing them within individual directories. Keeping the test
name as `test.s` will allow the Makefile to pickup your custom test and compile it.  
