SRCDIR=src
BUILDDIR=build
TESTDIR=test
TOOLSDIR=tools
INCLUDE=-I./src

obj:
	gcc -g3 ${INCLUDE} -c ${SRCDIR}/std.c -o ${BUILDDIR}/std.o
	gcc -g3 ${INCLUDE} -c ${SRCDIR}/minq.c -o ${BUILDDIR}/minq.o
	gcc -g3 ${INCLUDE} -c ${SRCDIR}/psscli.c -o ${BUILDDIR}/psscli.o 
	gcc -g3 ${INCLUDE} -c ${SRCDIR}/config.c -o ${BUILDDIR}/config.o 
	gcc -g3 ${INCLUDE} -c ${SRCDIR}/util.c -o ${BUILDDIR}/util.o 
	gcc ${CFLAGS} -g3 ${INCLUDE} -c ${SRCDIR}/ws.c -o ${BUILDDIR}/ws.o
	gcc ${CFLAGS} -g3 ${INCLUDE} -c ${SRCDIR}/server.c -o ${BUILDDIR}/server.o
	gcc -g3 ${INCLUDE} -c ${SRCDIR}/cmd.c -o ${BUILDDIR}/cmd.o
	gcc -g3 ${INCLUDE} -c ${SRCDIR}/error.c -o ${BUILDDIR}/error.o
	#gcc -g3 ${INCLUDE} -c ${SRCDIR}/callback.c -o ${BUILDDIR}/callback.o
	gcc -g3 ${INCLUDE} -c ${SRCDIR}/sync.c -o ${BUILDDIR}/sync.o
	gcc -g3 ${INCLUDE} -c ${SRCDIR}/crypt.c -o ${BUILDDIR}/crypt.o

test-ws: obj
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_ws.o -c ${TESTDIR}/node/ws.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_ws ${BUILDDIR}/test_ws.o ${BUILDDIR}/ws.o ${BUILDDIR}/cmd.o ${BUILDDIR}/minq.o ${BUILDDIR}/config.o ${BUILDDIR}/sync.o -lwebsockets -ljson-c -lpthread

test-baseaddr: obj
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/baseaddr.o -c ${TESTDIR}/node/baseaddr.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_baseaddr ${BUILDDIR}/baseaddr.o ${BUILDDIR}/ws.o ${BUILDDIR}/cmd.o ${BUILDDIR}/sync.o ${BUILDDIR}/minq.o -lwebsockets -ljson-c -lpthread

test-loadpeers: obj
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/loadpeers.o -c ${TESTDIR}/unit/loadpeers.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_loadpeers ${BUILDDIR}/loadpeers.o ${BUILDDIR}/psscli.o ${BUILDDIR}/util.o -ljson-c

test-server: obj
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_server.o -c ${TESTDIR}/node/server.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_server ${BUILDDIR}/test_server.o ${BUILDDIR}/server.o ${BUILDDIR}/std.o ${BUILDDIR}/cmd.o ${BUILDDIR}/ws.o $(BUILDDIR)/config.o $(BUILDDIR)/sync.o -lwebsockets -lpthread -ljson-c

test-smoke: obj
	gcc -g3 ${CFLAGS} ${INCLUDE} -o ${BUILDDIR}/test_smoke.o -c ${TESTDIR}/node/smoke.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_smoke ${BUILDDIR}/test_smoke.o ${BUILDDIR}/server.o ${BUILDDIR}/std.o ${BUILDDIR}/cmd.o ${BUILDDIR}/ws.o $(BUILDDIR)/config.o $(BUILDDIR)/minq.o $(BUILDDIR)/callback.o ${BUILDDIR}/sync.o -lwebsockets -lpthread -ljson-c

test-unit: obj test-tmp
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_minq.o -c ${TESTDIR}/unit/minq.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_minq ${BUILDDIR}/test_minq.o ${BUILDDIR}/minq.o
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_cmd.o -c ${TESTDIR}/unit/cmd.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_cmd ${BUILDDIR}/test_cmd.o ${BUILDDIR}/cmd.o ${BUILDDIR}/minq.o
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_ws.o -c ${TESTDIR}/unit/ws.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_ws ${BUILDDIR}/test_ws.o ${BUILDDIR}/cmd.o ${BUILDDIR}/minq.o ${BUILDDIR}/ws.o ${BUILDDIR}/sync.o -ljson-c -lwebsockets -lpthread
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_server.o -c ${TESTDIR}/unit/server.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_server ${BUILDDIR}/test_server.o ${BUILDDIR}/cmd.o ${BUILDDIR}/minq.o ${BUILDDIR}/server.o ${BUILDDIR}/config.o ${BUILDDIR}/sync.o -lpthread

test-tmp: obj
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_crypt.o -c ${TESTDIR}/unit/crypt.c
	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/test_crypt ${BUILDDIR}/test_crypt.o ${BUILDDIR}/crypt.o -lsodium

#
#tools: obj
#	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/pssd.o -c ${TOOLSDIR}/pssd.c
#	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/pssd ${BUILDDIR}/pssd.o ${BUILDDIR}/config.o ${BUILDDIR}/ws.o ${BUILDDIR}/server.o ${BUILDDIR}/std.o ${BUILDDIR}/cmd.o $(BUILDDIR)/error.o -lwebsockets -lpthread -ljson-c
#

tools: obj
#	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/pss-addr.o -c ${TOOLSDIR}/baseaddr.c
#	gcc -g3 ${INCLUDE} -o ${BUILDDIR}/pss-addr ${BUILDDIR}/pss-addr.o ${BUILDDIR}/config.o ${BUILDDIR}/server.o ${BUILDDIR}/ws.o ${BUILDDIR}/cmd.o ${BUILDDIR}/std.o $(BUILDDIR)/error.o -lwebsockets -ljson-c


.PHONY: clean
clean:
	rm -rf ${BUILDDIR}/*
