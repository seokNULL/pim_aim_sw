CC ?= g++

CFLAGS += -g
#CFLAGS += -O2 -fno-inline -Wall -Wstrict-prototypes
CFLAGS += -I. -I../include -I../dma-utils
CFLAGS += $(EXTRA_FLAGS)

TARGET = ../../bin/memcpy
TARGET2 = memcpy
OBJS = memcpy.o

OBJECTS = $(patsubst %.cpp,%.o, $(wildcard ${PWD}/*.cpp))

# Default target executed when no arguments are given to make.
.DEFAULT_GOAL=all

${TARGET}: ${OBJS}
	${CXX} ${CXXFLAGS} -o $@ $^ ${LIBS}
${TARGET2}: ${OBJS}
	${CXX} ${CXXFLAGS} -o $@ $^ ${LIBS}

all: ${TARGET2}
.PHONY: all

test: ${TARGET}
.PHONY: test

clean:
	rm -f *.o *.d ${TARGET2}
	rm -f *.o *.d ${TARGET}