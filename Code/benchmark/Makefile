CC=clang++
CFLAGS=-Wall -g -O3 -mno-omit-leaf-frame-pointer -fno-omit-frame-pointer
SANITIZERS=-fsanitize=address -fsanitize=undefined

.SUFFIXES: .o .cpp .h .cc

SRC_DIRS = ./system/ 
DEPS = -I$(BOOST)

PROTO_SOURCES = $(wildcard ./protobuf/*.proto)

CFLAGS += $(DEPS)
LDFLAGS = $(CFLAGS)
LIBS = -lanl -ldl -lprotobuf -lpthread -latomic
QUEUE_SOURCES = $(wildcard ./*.cpp ./*.cc)

QUEUE_OBJ = $(QUEUE_SOURCES:.cpp=.o)


.PHONY: all clean format proto
all: testQueue

testQueue : $(QUEUE_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
%.o : %.cpp
	$(CC) -c ${CFLAGS} -o $@ $<

proto: $(PROTO_SOURCES)
	protoc -I./protobuf --cpp_out=./ ./protobuf/*

%.o : %.cpp
	$(CC) -c ${CFLAGS} -I$(SRC_DIRS)  -o $@ $<