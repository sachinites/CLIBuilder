CC=g++
CFLAGS=-g -fpermissive -Wall -gdwarf-2 -g3 -Wignored-qualifiers
INCLUDES=-I .
CLILIB=libcli.a
TARGET:${CLILIB} testapp.exe
OBJ=KeyProcessor/KeyProcessor.o \
		 string_util.o \
		 serializer/serialize.o \
		 stack/stack.o \
		 CmdTree/CmdTree.o \
		 CmdTree/clistd.o \
		 CmdTree/CmdTreeCursor.o \
		 gluethread/glthread.o \
		 # printf_hijack.o \
		 
KeyProcessor/KeyProcessor.o:KeyProcessor/KeyProcessor.cpp
	@echo "Building KeyProcessor/KeyProcessor.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} KeyProcessor/KeyProcessor.cpp -o KeyProcessor/KeyProcessor.o

serializer/serialize.o:serializer/serialize.c
	@echo "Building serialize.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} serializer/serialize.c -o serializer/serialize.o

stack/stack.o:stack/stack.c
	@echo "Building stack/stack.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} stack/stack.c -o stack/stack.o

gluethread/glthread.o:gluethread/glthread.c
	@echo "Building gluethread/glthread.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} gluethread/glthread.c -o gluethread/glthread.o

string_util.o:string_util.c
	@echo "Building string_util.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} string_util.c -o string_util.o

CmdTree/clistd.o:CmdTree/clistd.cpp
	@echo "Building CmdTree/clistd.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} CmdTree/clistd.cpp -o CmdTree/clistd.o

CmdTree/CmdTreeCursor.o:CmdTree/CmdTreeCursor.cpp
	@echo "Building CmdTree/CmdTreeCursor.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} CmdTree/CmdTreeCursor.cpp -o CmdTree/CmdTreeCursor.o

CmdTree/CmdTree.o:CmdTree/CmdTree.cpp
	@echo "Building CmdTree/CmdTree.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} CmdTree/CmdTree.cpp -o CmdTree/CmdTree.o

testapp.o:testapp.c
	@echo "Building testapp.o"
	@ ${CC} ${CFLAGS} -c ${INCLUDES} testapp.c -o testapp.o

${CLILIB}: ${OBJ}
	@echo "Building Library ${CLILIB}"
	ar rs ${CLILIB} ${OBJ}

testapp.exe:testapp.o ${CLILIB}
	@echo "Building testapp.exe"
	${CC} -g testapp.o -o testapp.exe -L . -lcli -lncurses
	@echo "Build Finished : " ${CLILIB} testapp.exe

clean:
	rm -f *.exe
	rm -f *.o
	rm -f CmdTree/*.o 
	rm -f KeyProcessor/*.o
	rm -f stack/*.o 
	rm -f gluethread/*.o 
	rm -f serializer/*.o 
	rm -f ${CLILIB}
install:
	cp ${CLILIB} /usr/local/lib/
	cp libcli.h /usr/include/
uninstall:
	rm -f /usr/local/lib/${CLILIB}
	rm -f /usr/include/libcli.h
