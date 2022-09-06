CC=g++
CFLAGS=-Wall -g -std=c++17
NNMSG=${PWD}/deps/nng
BOOST=${PWD}/deps/boost_1_80_0

.SUFFIXES: .o .cpp .h

SRC_DIRS = ./system/ 
DEPS = -I$(NNMSG)/include -I$(BOOST) 

CFLAGS += $(DEPS) 
LDFLAGS = -L$(NNMSG)/lib 
LDFLAGS += $(CFLAGS)
LIBS = -lnng -lanl -ldl -lpthread


SOURCES = $(wildcard ./system/*.cpp)
SOURCES += $(wildcard ./configuration/*.cpp)
SOURCES_OBJ = $(SOURCES:.cpp=.o)

#abc:
#	echo ${SOURCES}
#	echo ${SOURCES_OBJ}

.PHONY: all
all: rundb

rundb : $(SOURCES_OBJ)
	$(CC) -static -o $@ $^ $(LDFLAGS) $(LIBS)
%.o : %.cpp
	$(CC) -c ${CFLAGS} -o $@ $<


#runcl : $(OBJS_CL)
#	$(CC) -static -o $@ $^ $(LDFLAGS) $(LIBS)
#./obj/%.o: system/%.cpp
#	$(CC) -c ${CFLAGS} $(INCLUDE) $(LIBS) -o $@ $<


.PHONY: clean
clean:
	rm -f system/*.o configuration/*.o rundb
