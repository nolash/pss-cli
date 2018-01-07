SRCDIR=src
BUILDDIR=build
TESTDIR=test
INCLUDE=-I./src

obj:
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/psscli.o -c ${SRCDIR}/psscli.c 
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/util.o -c ${SRCDIR}/util.c 
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/ws.o -c ${SRCDIR}/ws.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/server.o -c ${SRCDIR}/server.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/std.o -c ${SRCDIR}/std.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/cmd.o -c ${SRCDIR}/cmd.c

test-connect: obj
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/connect.o -c ${TESTDIR}/connect.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_connect ${BUILDDIR}/connect.o ${BUILDDIR}/ws.o -lwebsockets

test-loadpeers: obj
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/loadpeers.o -c ${TESTDIR}/loadpeers.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_loadpeers ${BUILDDIR}/loadpeers.o ${BUILDDIR}/psscli.o ${BUILDDIR}/util.o -ljson-c

test-server: obj
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_server.o -c ${TESTDIR}/server.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_server ${BUILDDIR}/test_server.o ${BUILDDIR}/server.o ${BUILDDIR}/std.o ${BUILDDIR}/cmd.o ${BUILDDIR}/ws.o -lwebsockets -lpthread -ljson-c

clean:
	rm -rf ${BUILDDIR}/*
