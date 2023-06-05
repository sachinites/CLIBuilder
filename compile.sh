g++ -g -c string_util.c -o string_util.o -fpermissive
g++ -g -c -I serializer serializer/serialize.c -o serializer/serialize.o -fpermissive
g++ -g -c -I stack stack/stack.c -o stack/stack.o -fpermissive
g++ -g -c -I . KeyProcessor/KeyProcessor.cpp -o KeyProcessor/KeyProcessor.o
g++ -g -c -I . CmdTree/CmdTree.cpp -o CmdTree/CmdTree.o
g++ -g -c -I . CmdTree/clistd.cpp -o CmdTree/clistd.o
g++ -g -c -I . CmdTree/CmdTreeCursor.cpp -o CmdTree/CmdTreeCursor.o
g++ -g -c -I . gluethread/glthread.c -o gluethread/glthread.o
g++ -g -c -I . app.cpp -o app.o
g++ -g -c -I . main4.c -o main4.o
g++ -g -c -I . testapp.c -o testapp.o -fpermissive
g++ -g app.o string_util.o serializer/serialize.o  stack/stack.o KeyProcessor/KeyProcessor.o CmdTree/CmdTree.o CmdTree/clistd.o gluethread/glthread.o CmdTree/CmdTreeCursor.o -o exe -lncurses
g++ -g main4.o string_util.o serializer/serialize.o  stack/stack.o KeyProcessor/KeyProcessor.o CmdTree/CmdTree.o CmdTree/clistd.o gluethread/glthread.o CmdTree/CmdTreeCursor.o -o main4.exe -lncurses
g++ -g testapp.o string_util.o serializer/serialize.o  stack/stack.o KeyProcessor/KeyProcessor.o CmdTree/CmdTree.o CmdTree/clistd.o gluethread/glthread.o CmdTree/CmdTreeCursor.o -o testapp.exe -lncurses