# CLIBuilder
Build Cisco like Command Line Interface 

g++ -g -c main.cpp -o main.o
g++ -g -c KeyProcessor/KeyProcessor.cpp -o KeyProcessor/KeyProcessor.o
g++ -g KeyProcessor/KeyProcessor.o main.o -o main.exe -lncurses

