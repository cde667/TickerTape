#
# Simple Makefile for now...
#

#CCC = g++
CCC = clang++

TEST_INC_DEP=   TestTape.h \
		TestShm.h \
		../MemoryTape.h \
		../TapeWriter.h \
		../TapeReader.h \
		../TapeSync.h\
		../TapeSync_impl.h \
		../TapeHeaderMessage.h \
		../detail/CacheLine.h \
		../detail/MessageSize.h \
		../detail/TapeReader.h \
		../detail/TapeWriter.h \
		../detail/Utils.h 

SOCKET_INC_DEP= ../sockets/SocketMsgs.h			\
		../sockets/SyncSocketListener_impl.h\
		../sockets/SyncSocketClient.h		\
		../sockets/SyncSocketServer.h\
		../sockets/SyncSocketClient_impl.h	\
		../sockets/SyncSocketServer_impl.h\
		../sockets/SyncSocketListener.h\


CFLAGS=-g -O0 -std=c++14

all: MsgSizeTest TestTapeWriter TapeWriter TapeReader  TapeSync

MsgSizeTest: MsgSizeTest.cpp ../detail/MessageSize.h
	$(CCC) $(CFLAGS) MsgSizeTest.cpp -o MsgSizeTest

TestTapeWriter: ../detail/TapeWriter.h TestTapeWriter.cpp
	$(CCC) $(CFLAGS) TestTapeWriter.cpp -lpthread -o TestTapeWriter

TapeWriter: $(TEST_INC_DEP) TapeWriter.cpp
	$(CCC) $(CFLAGS) TapeWriter.cpp -lpthread -o TapeWriter

TapeReader: $(TEST_INC_DEP) TapeReader.cpp
	$(CCC) $(CFLAGS) TapeReader.cpp -lpthread -o TapeReader

TapeSync: $(TEST_INC_DEP) $(SOCKET_INC_DEP) TapeSync.cpp
	$(CCC) $(CFLAGS) TapeSync.cpp -lpthread -o TapeSync

clean:
	rm -f  MsgSizeTest TestTapeWriter TapeWriter TapeReader TapeSync
	rm -rf MsgSizeTest.dSYM TapeReader.dSYM TapeWriter.dSYM TestTapeWriter.dSYM TapeSync.dSYM
