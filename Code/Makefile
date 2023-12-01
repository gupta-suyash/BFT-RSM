CC=clang++
CFLAGS=-Wall -g -std=c++20 -O3 -mno-omit-leaf-frame-pointer -fno-omit-frame-pointer -flto
NNMSG=${PWD}/deps/nng
BOOST=${PWD}/deps/boost_1_80_0
SANITIZERS=-fsanitize=address -fsanitize=undefined

.SUFFIXES: .o .cpp .h .cc

SRC_DIRS = ./system/ 
DEPS = -I$(NNMSG)/include -I$(BOOST)

PROTO_DIR = ./protobuf/

CFLAGS += $(DEPS)
LDFLAGS = -L$(NNMSG)/lib 
LDFLAGS += $(CFLAGS)
LIBS = -lnng -lanl -ldl -lprotobuf -lpthread -ljsoncpp -latomic -lcryptopp

PROTO_SOURCES = $(wildcard ./protobuf/*.proto)
SOURCES = $(wildcard ./system/*.cpp ./system/*.cc)
SOURCES += $(wildcard ./configuration/*.cpp)
LIBRARY_SOURCES = $(filter-out ./system/main.cpp, $(SOURCES))
TEST_SOURCES = $(wildcard ./test/*.cpp ./test/*.cc)

SOURCES_OBJ = $(SOURCES:.cpp=.o)
LIBRARY_SOURCES_OBJ = $(LIBRARY_SOURCES:.cpp=.o)
TEST_SOURCES_OBJ = $(TEST_SOURCES:.cpp=.o)

FORMAT_TARGET_REGEX = '.*\.\(cpp\|c\|h\|.proto\)'

.PHONY: all clean format test proto
all: scrooge

scrooge : $(SOURCES_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
%.o : %.cpp
	$(CC) -c ${CFLAGS} -o $@ $<

proto: $(PROTO_SOURCES)
	protoc -I./protobuf --cpp_out=./system ./protobuf/*
	protoc -I./protobuf --go_out=./client ./protobuf/*

clean:
	rm -f system/*.o configuration/*.o scrooge test/*.o check system/*.pb* client/proto*.pb*

format:
	find system -regex $(FORMAT_TARGET_REGEX) -exec clang-format -i -style=Microsoft {} \;
	find configuration -regex $(FORMAT_TARGET_REGEX) -exec clang-format -i -style=Microsoft {} \;
	find test -regex $(FORMAT_TARGET_REGEX) -exec clang-format -i -style=Microsoft {} \;

check : $(LIBRARY_SOURCES_OBJ) $(TEST_SOURCES_OBJ)
	$(CC) -o $@ $^ -I$(SRC_DIRS) $(LDFLAGS) $(LIBS)
	./check -l unit_scope
%.o : %.cpp
	$(CC) -c ${CFLAGS} -I$(SRC_DIRS)  -o $@ $<
