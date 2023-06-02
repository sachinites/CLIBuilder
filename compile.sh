g++ -g -c string_util.c -o string_util.o -fpermissive
g++ -g -c -I serializer serializer/serialize.c -o serializer/serialize.o -fpermissive
g++ -g -c -I stack stack/stack.c -o stack/stack.o -fpermissive
g++ -g -c -I . KeyProcessor/KeyProcessor.cpp -o KeyProcessor/KeyProcessor.o
g++ -g -c -I . CmdParser/CmdParser.cpp -o CmdParser/CmdParser.o
g++ -g -c -I . CmdTree/CmdTree.cpp -o CmdTree/CmdTree.o
g++ -g -c -I . CmdTree/clistd.cpp -o CmdTree/clistd.o
g++ -g string_util.o serializer/serialize.o  stack/stack.o KeyProcessor/KeyProcessor.o CmdParser/CmdParser.o CmdTree/CmdTree.o CmdTree/clistd.o -o exe -lncurses