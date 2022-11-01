CC=g++
CFLAGS=-Wall -g -std=c++17
NNMSG=${PWD}/deps/nng
BOOST=${PWD}/deps/boost_1_80_0

.SUFFIXES: .o .cpp .h

SRC_DIRS = ./system/ 
DEPS = -I$(NNMSG)/include -I$(BOOST) 

PROTO_DIR = ./system/protobuf/

CFLAGS += $(DEPS) 
LDFLAGS = -L$(NNMSG)/lib 
LDFLAGS += $(CFLAGS)
LIBS = -lnng -lanl -ldl -lprotobuf -lpthread

SOURCES = $(wildcard ./system/*.cpp ./system/*.cc)
SOURCES += $(wildcard ./configuration/*.cpp)
SOURCES_OBJ = $(SOURCES:.cpp=.o)
FORMAT_TARGET_REGEX = '.*\.\(cpp\|c\|h\|.proto\)'

#abc:
#	echo ${SOURCES}
#	echo ${SOURCES_OBJ}

.PHONY: all
all: rundb

rundb : $(SOURCES_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
%.o : %.cpp
	$(CC) -c ${CFLAGS} -o $@ $<

# protobuf: protoc -I=$(PROTO_DIR) --cpp_out=$(SRC_DIRS) $(PROTO_DIR)/crosschainmessage.proto

#runcl : $(OBJS_CL)
#	$(CC) -static -o $@ $^ $(LDFLAGS) $(LIBS)
#./obj/%.o: system/%.cpp
#	$(CC) -c ${CFLAGS} $(INCLUDE) $(LIBS) -o $@ $<


.PHONY: clean
clean:
	rm -f system/*.o configuration/*.o rundb

format:
	find system -regex $(FORMAT_TARGET_REGEX) -exec clang-format -i -style=Microsoft {} \;
	find configuration -regex $(FORMAT_TARGET_REGEX) -exec clang-format -i -style=Microsoft {} \;
