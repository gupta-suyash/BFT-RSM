CC=clang++
CFLAGS=-Wall -g -std=c++20 -O3 -mno-omit-leaf-frame-pointer -fno-omit-frame-pointer
SANITIZERS=-fsanitize=address -fsanitize=undefined

.SUFFIXES: .o .cpp .h .cc

SRC_DIRS = ./system/ 
DEPS = -I$(BOOST)

PROTO_DIR = ./protobuf/

CFLAGS += $(DEPS)
LDFLAGS = $(CFLAGS)
LIBS = -lprotobuf -lpthread -latomic
QUEUE_SOURCES = $(wildcard ./*.cpp ../system/*.cc)

QUEUE_OBJ = $(QUEUE_SOURCES:.cpp=.o)


.PHONY: all clean format 
all: testQueue

testQueue : $(QUEUE_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
%.o : %.cpp
	$(CC) -c ${CFLAGS} -o $@ $<

clean:
	rm -f system/*.o configuration/*.o scrooge test/*.o check system/*.pb* client/proto*.pb*

%.o : %.cpp
	$(CC) -c ${CFLAGS} -I$(SRC_DIRS)  -o $@ $<