CXX=g++ 
INC=
LIBS=-lpim
# CXXFLAGS += ${INC} -std=c++11 -g -O2
CXXFLAGS += ${INC} -std=c++11 -g
TARGET = ../../bin/matmul
TARGET2 = matmul
OBJS = matmul.o convert_numeric.o

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
