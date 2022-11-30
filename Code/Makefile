CC=g++
CFLAGS=-Wall -g -std=c++17
NNMSG=${PWD}/deps/nng
BOOST=${PWD}/deps/boost_1_80_0
SANITIZERS=-fsanitize=address -fsanitize=undefined

.SUFFIXES: .o .cpp .h

SRC_DIRS = ./system/ 
DEPS = -I$(NNMSG)/include -I$(BOOST) 

PROTO_DIR = ./system/protobuf/

CFLAGS += $(DEPS)
LDFLAGS = -L$(NNMSG)/lib 
LDFLAGS += $(CFLAGS)
LIBS = -lnng -lanl -ldl -lprotobuf -lpthread

PROTO_SOURCES = $(wildcard ./system/protobuf/*.proto)
SOURCES = $(wildcard ./system/*.cpp ./system/*.cc)
SOURCES += $(wildcard ./configuration/*.cpp)
LIBRARY_SOURCES = $(filter-out ./system/main.cpp, $(SOURCES))
TEST_SOURCES = $(wildcard ./test/*.cpp ./test/*.cc)

SOURCES_OBJ = $(SOURCES:.cpp=.o)
LIBRARY_SOURCES_OBJ = $(LIBRARY_SOURCES:.cpp=.o)
TEST_SOURCES_OBJ = $(TEST_SOURCES:.cpp=.o)

FORMAT_TARGET_REGEX = '.*\.\(cpp\|c\|h\|.proto\)'

#abc:
#	echo ${SOURCES}
#	echo ${SOURCES_OBJ}

.PHONY: all clean format test proto
all: proto scrooge

scrooge : $(SOURCES_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(SANITIZERS) $(LIBS) 
%.o : %.cpp
	$(CC) -c ${CFLAGS} $(SANITIZERS) -o $@ $<

# protobuf: protoc -I=$(PROTO_DIR) --cpp_out=$(SRC_DIRS) $(PROTO_DIR)/crosschainmessage.proto

#runcl : $(OBJS_CL)
#	$(CC) -static -o $@ $^ $(LDFLAGS) $(LIBS)
#./obj/%.o: system/%.cpp
#	$(CC) -c ${CFLAGS} $(INCLUDE) $(LIBS) -o $@ $<

proto: $(PROTO_SOURCES)
	protoc -I./system/protobuf --cpp_out=./system ./system/protobuf/*
	protoc -I./system/protobuf --go_out=./client ./system/protobuf/*

clean:
	rm -f system/*.o configuration/*.o scrooge test/*.o check system/*pb*

format:
	find system -regex $(FORMAT_TARGET_REGEX) -exec clang-format -i -style=Microsoft {} \;
	find configuration -regex $(FORMAT_TARGET_REGEX) -exec clang-format -i -style=Microsoft {} \;
	find test -regex $(FORMAT_TARGET_REGEX) -exec clang-format -i -style=Microsoft {} \;

check : $(LIBRARY_SOURCES_OBJ) $(TEST_SOURCES_OBJ)
	$(CC) -o $@ $^ -I$(SRC_DIRS) $(LDFLAGS) $(LIBS)
	./check -l unit_scope
%.o : %.cpp
	$(CC) -c ${CFLAGS} -I$(SRC_DIRS)  -o $@ $<
